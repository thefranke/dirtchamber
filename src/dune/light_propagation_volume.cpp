/*
 * Dune D3D library - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "light_propagation_volume.h"

#include "d3d_tools.h"
#include "mesh.h"
#include "unicode.h"
#include "gbuffer.h"

#define NUM_VPLS 1024

namespace dune
{
    struct lpv_vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 texcoord;

        void init(DirectX::XMFLOAT3& p, DirectX::XMFLOAT3& t)
        {
            position = p;
            texcoord = t;
        }
    };

    light_propagation_volume::light_propagation_volume() :
        time_inject_(0),
        time_propagate_(0),
        time_normalize_(0),
        bs_inject_(nullptr),
        bs_propagate_(nullptr),
        ss_vplfilter_(),
        vs_inject_(nullptr),
        gs_inject_(nullptr),
        ps_inject_(nullptr),
        vs_propagate_(nullptr),
        gs_propagate_(nullptr),
        ps_propagate_(nullptr),
        ps_normalize_(nullptr),
        propagate_start_slot_(-1),
        inject_rsm_start_slot_(-1),
        lpv_volume_(nullptr),
        input_layout_(nullptr),
        volume_size_(0),
        lpv_r_(),
        lpv_g_(),
        lpv_b_(),
        lpv_accum_r_(),
        lpv_accum_g_(),
        lpv_accum_b_(),
        lpv_inject_counter_(),
        iterations_rendered_(0),
        curr_(0),
        next_(0),
        cb_debug_(),
        cb_parameters_(),
        cb_propagation_(),
        cb_gi_parameters_(),
        profiler_()
    {
    }

    void light_propagation_volume::create(ID3D11Device* device, UINT volume_size)
    {
        volume_size_ = volume_size;

        profiler_.create(device);

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.Width = volume_size;
        desc.Height = volume_size;
        desc.ArraySize = volume_size;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        // create LPV 32x32x32
        for (size_t i = 0; i < 2; ++i)
        {
            lpv_r_[i].create(device, desc);
            lpv_g_[i].create(device, desc);
            lpv_b_[i].create(device, desc);
        }

        lpv_accum_r_.create(device, desc);
        lpv_accum_g_.create(device, desc);
        lpv_accum_b_.create(device, desc);

        // create normalization volume that keeps track of the number of lights in each cell
        desc.Format = DXGI_FORMAT_R16_FLOAT;

        lpv_inject_counter_.create(device, desc);

        // create grid geometry
        UINT num_vertices = 6 * volume_size_;
        std::vector<lpv_vertex> data(num_vertices);

        for(size_t d = 0; d < volume_size_; ++d)
        {
            data[d*6+0].init(DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, static_cast<float>(d)));
            data[d*6+1].init(DirectX::XMFLOAT3(-1.0f, 1.0f,  0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, static_cast<float>(d)));
            data[d*6+2].init(DirectX::XMFLOAT3(1.0f, -1.0f,  0.0f), DirectX::XMFLOAT3(1.0f, 1.0f, static_cast<float>(d)));

            data[d*6+3].init(DirectX::XMFLOAT3(1.0f, -1.0f,  0.0f), DirectX::XMFLOAT3(1.0f, 1.0f, static_cast<float>(d)));
            data[d*6+4].init(DirectX::XMFLOAT3(-1.0f, 1.0f,  0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, static_cast<float>(d)));
            data[d*6+5].init(DirectX::XMFLOAT3(1.0f, 1.0f,   0.0f), DirectX::XMFLOAT3(1.0f, 0.0f, static_cast<float>(d)));
        }

        D3D11_BUFFER_DESC bd;
        D3D11_SUBRESOURCE_DATA InitData;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(lpv_vertex) * num_vertices;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;
        InitData.pSysMem = &data[0];
        assert_hr(device->CreateBuffer( &bd, &InitData, &lpv_volume_));

        D3D11_BLEND_DESC bld;
        ZeroMemory(&bld, sizeof(D3D11_BLEND_DESC));

        bld.IndependentBlendEnable = TRUE;

        for (size_t i = 0; i < 6; ++i)
        {
            bld.RenderTarget[i].BlendEnable =    TRUE;
            bld.RenderTarget[i].SrcBlend =       D3D11_BLEND_ONE;
            bld.RenderTarget[i].DestBlend =      D3D11_BLEND_ONE;
            bld.RenderTarget[i].SrcBlendAlpha =  D3D11_BLEND_ONE;
            bld.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ONE;
            bld.RenderTarget[i].BlendOp =        D3D11_BLEND_OP_ADD;
            bld.RenderTarget[i].BlendOpAlpha =   D3D11_BLEND_OP_ADD;
            bld.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        assert_hr(device->CreateBlendState(&bld, &bs_inject_));

        for (size_t i = 0; i < 3; ++i)
            bld.RenderTarget[i].BlendEnable = FALSE;

        assert_hr(device->CreateBlendState(&bld, &bs_propagate_));

        D3D11_SAMPLER_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.MaxAnisotropy = 1;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MaxLOD = D3D11_FLOAT32_MAX;

        ss_vplfilter_.create(device, sd);

        cb_parameters_.create(device);
        cb_debug_.create(device);
        cb_propagation_.create(device);
        cb_gi_parameters_.create(device);

        DirectX::XMStoreFloat4x4(&world_to_lpv_, DirectX::XMMatrixIdentity());

        curr_ = 0;
        next_ = 1;

        iterations_rendered_ = 0;
    }

    void light_propagation_volume::set_model_matrix(ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& model, const DirectX::XMFLOAT3& lpv_min, const DirectX::XMFLOAT3& lpv_max, UINT lpv_parameters_slot)
    {
        DirectX::XMMATRIX model_inv = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&model));

        DirectX::XMVECTOR diag = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&lpv_max), DirectX::XMLoadFloat3(&lpv_min));

        DirectX::XMFLOAT3 d;
        DirectX::XMStoreFloat3(&d, diag);

        DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(1.f/d.x, 1.f/d.y, 1.f/d.z);

        DirectX::XMMATRIX trans = DirectX::XMMatrixTranslation(-lpv_min.x, -lpv_min.y, -lpv_min.z);

        DirectX::XMMATRIX world_to_lpv = model_inv * trans * scale;
        DirectX::XMStoreFloat4x4(&world_to_lpv_, world_to_lpv);

        auto cb = &cb_parameters_.data();
        {
            DirectX::XMStoreFloat4x4(&cb->world_to_lpv,
                world_to_lpv);

            cb->lpv_size = volume_size_;
        }
        cb_parameters_.to_vs(context, lpv_parameters_slot);
        cb_parameters_.to_ps(context, lpv_parameters_slot);
    }

    void light_propagation_volume::destroy()
    {
        for (size_t i = 0; i < 2; ++i)
        {
            lpv_r_[i].destroy();
            lpv_g_[i].destroy();
            lpv_b_[i].destroy();
        }

        lpv_accum_r_.destroy();
        lpv_accum_g_.destroy();
        lpv_accum_b_.destroy();

        lpv_inject_counter_.destroy();

        cb_parameters_.destroy();
        cb_debug_.destroy();
        cb_propagation_.destroy();
        cb_gi_parameters_.destroy();

        safe_release(input_layout_);
        safe_release(lpv_volume_);

        safe_release(bs_inject_);

        safe_release(bs_propagate_);

        safe_release(vs_inject_);
        safe_release(gs_inject_);
        safe_release(ps_inject_);

        safe_release(vs_propagate_);
        safe_release(gs_propagate_);
        safe_release(ps_propagate_);

        safe_release(ps_normalize_);

        ss_vplfilter_.destroy();

        profiler_.destroy();

        propagate_start_slot_ = -1;
        inject_rsm_start_slot_ = -1;
        curr_ = next_ = 0;
    }

    void light_propagation_volume::swap_buffers()
    {
        std::swap(curr_, next_);
    }

    void light_propagation_volume::inject(ID3D11DeviceContext* context, gbuffer& rsm, bool clear)
    {
        dune::set_viewport(context, volume_size_, volume_size_);

        profiler_.begin(context);

        // TODO: this should be rsm->size() * rsm->size()!
        unsigned int num_vpls = static_cast<unsigned int>(NUM_VPLS*NUM_VPLS);

        rsm.to_vs(context, inject_rsm_start_slot_);

        // render into lpv
        ID3D11RenderTargetView* lpv_views[] =
        {
            lpv_r_[next_].rtv(),
            lpv_g_[next_].rtv(),
            lpv_b_[next_].rtv(),
            lpv_inject_counter_.rtv(),
        };

        static float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        dune::clear_rtvs(context, lpv_views, 4, clear_color);

        context->OMSetRenderTargets(4, lpv_views, nullptr);

        FLOAT factors[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        context->OMSetBlendState(bs_inject_, factors, 0xffffffff);

        // create pseudo buffer and call shader for each pixel in the rsm
        unsigned int stride = 0;
        unsigned int offset = 0;
        ID3D11Buffer* buffer[] = { nullptr };

        context->IASetVertexBuffers(0, 1, buffer, &stride, &offset);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

        ss_vplfilter_.to_vs(context, 0);

        context->VSSetShader(vs_inject_, nullptr, 0);
        context->GSSetShader(gs_inject_, nullptr, 0);
        context->PSSetShader(ps_inject_, nullptr, 0);

        context->Draw(num_vpls, 0);

        // clear and done
        ID3D11RenderTargetView* null_views[] = { nullptr, nullptr, nullptr, nullptr };
        context->OMSetRenderTargets(4, null_views, nullptr);

        ID3D11ShaderResourceView* null_views1[] = { nullptr, nullptr, nullptr };
        context->VSSetShaderResources(inject_rsm_start_slot_, 3, null_views1);

        time_inject_ = profiler_.result();
    }

    void light_propagation_volume::normalize(ID3D11DeviceContext* context)
    {
        swap_buffers();

        ID3D11RenderTargetView* lpv_views[] =
        {
            lpv_r_[next_].rtv(),
            lpv_g_[next_].rtv(),
            lpv_b_[next_].rtv(),
        };

        static float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        dune::clear_rtvs(context, lpv_views, 3, clear_color);

        context->OMSetRenderTargets(3, lpv_views, nullptr);

        FLOAT factors[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        context->OMSetBlendState(bs_inject_, factors, 0xffffffff);

        UINT stride = sizeof(lpv_vertex);
        UINT offset = 0;

        context->IASetInputLayout(input_layout_);
        context->IASetVertexBuffers(0, 1, &lpv_volume_, &stride, &offset);
        context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT,0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        context->VSSetShader(vs_propagate_, nullptr, 0);
        context->GSSetShader(gs_propagate_, nullptr, 0);
        context->PSSetShader(ps_normalize_, nullptr, 0);

        ID3D11ShaderResourceView* sr_lpv[] =
        {
            lpv_r_[curr_].srv(),
            lpv_g_[curr_].srv(),
            lpv_b_[curr_].srv(),
            lpv_inject_counter_.srv(),
        };

        context->PSSetShaderResources(propagate_start_slot_, 4, sr_lpv);

        context->Draw(6 * volume_size_, 0);

        // clear and done
        ID3D11RenderTargetView* rt_null_views[] = { nullptr, nullptr, nullptr };
        context->OMSetRenderTargets(3, rt_null_views, nullptr);

        ID3D11ShaderResourceView* sr_null_views[] = { nullptr, nullptr, nullptr, nullptr };
        context->PSSetShaderResources(propagate_start_slot_, 4, sr_null_views);
    }

    void light_propagation_volume::propagate(ID3D11DeviceContext* context)
    {
        swap_buffers();

        ID3D11RenderTargetView* lpv_views[] =
        {
            lpv_r_[next_].rtv(),
            lpv_g_[next_].rtv(),
            lpv_b_[next_].rtv(),
            lpv_accum_r_.rtv(),
            lpv_accum_g_.rtv(),
            lpv_accum_b_.rtv(),
        };

        static float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        dune::clear_rtvs(context, lpv_views, 3, clear_color);

        context->OMSetRenderTargets(6, lpv_views, nullptr);

        ID3D11ShaderResourceView* sr_lpv[] =
        {
            lpv_r_[curr_].srv(),
            lpv_g_[curr_].srv(),
            lpv_b_[curr_].srv()
        };

        context->PSSetShaderResources(propagate_start_slot_, 3, sr_lpv);

        context->Draw(6 * volume_size_, 0);

        // clear and done
        ID3D11RenderTargetView* rt_null_views[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
        context->OMSetRenderTargets(6, rt_null_views, nullptr);

        ID3D11ShaderResourceView* sr_null_views[] = { nullptr, nullptr, nullptr };
        context->PSSetShaderResources(propagate_start_slot_, 3, sr_null_views);
    }

    void light_propagation_volume::to_ps(ID3D11DeviceContext* context, UINT lpv_out_start_slot)
    {
        if (iterations_rendered_ > 0)
        {
            ID3D11ShaderResourceView* views[] = { lpv_accum_r_.srv(), lpv_accum_g_.srv(), lpv_accum_b_.srv() };
            context->PSSetShaderResources(lpv_out_start_slot, 3, views);
        }
        else
        {
            ID3D11ShaderResourceView* views[] = { lpv_r_[next_].srv(), lpv_g_[next_].srv(), lpv_b_[next_].srv() };
            context->PSSetShaderResources(lpv_out_start_slot, 3, views);
        }
    }

    void light_propagation_volume::propagate(ID3D11DeviceContext* context, size_t num_iterations)
    {
        if (num_iterations == 0)
            return;

        static float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        ID3D11RenderTargetView* lpv_accum_views[] =
        {
            lpv_accum_r_.rtv(),
            lpv_accum_g_.rtv(),
            lpv_accum_b_.rtv(),
        };

        dune::clear_rtvs(context, lpv_accum_views, 3, clear_color);

        UINT stride = sizeof(lpv_vertex);
        UINT offset = 0;

        context->IASetInputLayout(input_layout_);
        context->IASetVertexBuffers(0, 1, &lpv_volume_, &stride, &offset);
        context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT,0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        FLOAT factors[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        context->OMSetBlendState(bs_propagate_, factors, 0xffffffff);

        context->VSSetShader(vs_propagate_, nullptr, 0);
        context->GSSetShader(gs_propagate_, nullptr, 0);
        context->PSSetShader(ps_propagate_, nullptr, 0);

        for (UINT i = 0; i < num_iterations; ++i)
        {
            auto c = &cb_propagation_.data();
            {
                c->iteration = i;
            }
            cb_propagation_.to_ps(context, 13);

            propagate(context);
        }
    }

    void light_propagation_volume::render(ID3D11DeviceContext* context)
    {
        dune::set_viewport(context, volume_size_, volume_size_);

        profiler_.begin(context);
        normalize(context);
        time_normalize_ = profiler_.result();

        profiler_.begin(context);
        propagate(context, iterations_rendered_);
        time_propagate_ = profiler_.result();
    }

    void light_propagation_volume::set_inject_shader(ID3D11VertexShader* vs, ID3D11GeometryShader* gs, ID3D11PixelShader* ps, ID3D11PixelShader* ps_normalize, UINT inject_rsm_start_slot)
    {
        exchange(&vs_inject_, vs);
        exchange(&ps_inject_, ps);
        exchange(&gs_inject_, gs);
        exchange(&ps_normalize_, ps_normalize);

        inject_rsm_start_slot_ = inject_rsm_start_slot;
    }

    void light_propagation_volume::set_propagate_shader(ID3D11Device* device, ID3D11VertexShader* vs, ID3D11GeometryShader* gs, ID3D11PixelShader* ps, ID3DBlob* input_binary, UINT start_slot)
    {
        exchange(&vs_propagate_, vs);
        exchange(&gs_propagate_, gs);
        exchange(&ps_propagate_, ps);

        propagate_start_slot_ = start_slot;

        // create input layout
        D3D11_INPUT_ELEMENT_DESC lpv_layout_desc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        safe_release(input_layout_);

        assert_hr(device->CreateInputLayout(lpv_layout_desc, ARRAYSIZE(lpv_layout_desc), input_binary->GetBufferPointer(),
                                            input_binary->GetBufferSize(), &input_layout_));
    }

    void light_propagation_volume::visualize(ID3D11DeviceContext* context, d3d_mesh* node, UINT debug_info_slot)
    {
        if (!node)
        {
            tcout << L"No node to render volume" << std::endl;
            return;
        }

        size_t f = 1;

        for (size_t z = 0; z < volume_size_; z+=f)
        for (size_t y = 0; y < volume_size_; y+=f)
        for (size_t x = 0; x < volume_size_; x+=f)
        {
            auto cb = &cb_debug_.data();
            {
                cb->lpv_pos = DirectX::XMFLOAT3(static_cast<float>(x),
                                                static_cast<float>(y),
                                                static_cast<float>(z));
            }
            cb_debug_.to_ps(context, debug_info_slot);
            cb_debug_.to_vs(context, debug_info_slot);

            node->render(context);
        }
    }

    delta_light_propagation_volume::delta_light_propagation_volume() :
        bs_negative_inject_(nullptr),
        vs_direct_inject_(nullptr),
        negative_(false),
        cb_delta_injection_()
    {
    }

    void delta_light_propagation_volume::inject(ID3D11DeviceContext* context, gbuffer& rsm, bool clear, bool is_direct, float scale)
    {
        // set delta injection parameters
        auto p = &cb_delta_injection_.data();
        {
            p->is_direct = false;
            p->scale = scale;
        }
        cb_delta_injection_.to_vs(context, 6);

        dune::set_viewport(context, volume_size_, volume_size_);

        profiler_.begin(context);

        unsigned int num_vpls = static_cast<unsigned int>(NUM_VPLS*NUM_VPLS);

        rsm.to_vs(context, inject_rsm_start_slot_);

        // render into lpv
        ID3D11RenderTargetView* lpv_views[] =
        {
            lpv_r_[next_].rtv(),
            lpv_g_[next_].rtv(),
            lpv_b_[next_].rtv(),
            lpv_inject_counter_.rtv(),
        };

        if (clear)
        {
            static float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            dune::clear_rtvs(context, lpv_views, 4, clear_color);
            negative_ = false;
        }

        context->OMSetRenderTargets(4, lpv_views, nullptr);

        FLOAT factors[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        if (!negative_)
            context->OMSetBlendState(bs_inject_, factors, 0xffffffff);
        else
            context->OMSetBlendState(bs_negative_inject_, factors, 0xffffffff);

        // create pseudo buffer and call shader for each pixel in the rsm
        unsigned int stride = 0;
        unsigned int offset = 0;
        ID3D11Buffer* buffer[] = { nullptr };

        context->IASetVertexBuffers(0, 1, buffer, &stride, &offset);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

        ss_vplfilter_.to_vs(context, 0);

        if (is_direct)
            context->VSSetShader(vs_direct_inject_, nullptr, 0);
        else
            context->VSSetShader(vs_inject_, nullptr, 0);

        context->GSSetShader(gs_inject_, nullptr, 0);
        context->PSSetShader(ps_inject_, nullptr, 0);

        context->Draw(num_vpls, 0);

        // clear and done
        ID3D11RenderTargetView* null_views[] = { nullptr, nullptr, nullptr, nullptr };
        context->OMSetRenderTargets(4, null_views, nullptr);

        ID3D11ShaderResourceView* null_views1[] = { nullptr, nullptr, nullptr };
        context->VSSetShaderResources(inject_rsm_start_slot_, 3, null_views1);

        negative_ = !negative_;

        time_inject_ = profiler_.result();
    }

    void delta_light_propagation_volume::propagate_direct(ID3D11DeviceContext* context, size_t num_iterations)
    {
        if (num_iterations == 0)
            return;

        static float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        ID3D11RenderTargetView* lpv_accum_views[] =
        {
            lpv_accum_r_.rtv(),
            lpv_accum_g_.rtv(),
            lpv_accum_b_.rtv(),
        };

        UINT stride = sizeof(lpv_vertex);
        UINT offset = 0;

        context->IASetInputLayout(input_layout_);
        context->IASetVertexBuffers(0, 1, &lpv_volume_, &stride, &offset);
        context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT,0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        FLOAT factors[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        context->OMSetBlendState(bs_propagate_, factors, 0xffffffff);

        context->VSSetShader(vs_propagate_, nullptr, 0);
        context->GSSetShader(gs_propagate_, nullptr, 0);
        context->PSSetShader(ps_propagate_, nullptr, 0);

        for (UINT i = 0; i < num_iterations; ++i)
        {
            auto c = &cb_propagation_.data();
            {
                c->iteration = i;
            }
            cb_propagation_.to_ps(context, 13);

            propagate(context);
        }
    }

    void delta_light_propagation_volume::create(ID3D11Device* device, UINT volume_size)
    {
        light_propagation_volume::create(device, volume_size);

        D3D11_BLEND_DESC bld;
        ZeroMemory(&bld, sizeof(D3D11_BLEND_DESC));

        bld.IndependentBlendEnable = TRUE;

        for (size_t i = 0; i < 6; ++i)
        {
            bld.RenderTarget[i].BlendEnable =    TRUE;
            bld.RenderTarget[i].SrcBlend =       D3D11_BLEND_ONE;
            bld.RenderTarget[i].DestBlend =      D3D11_BLEND_ONE;
            bld.RenderTarget[i].SrcBlendAlpha =  D3D11_BLEND_ONE;
            bld.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ONE;
            bld.RenderTarget[i].BlendOp =        D3D11_BLEND_OP_ADD;
            bld.RenderTarget[i].BlendOpAlpha =   D3D11_BLEND_OP_ADD;
            bld.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        for (size_t i = 0; i < 3; ++i)
        {
            bld.RenderTarget[i].BlendOp =        D3D11_BLEND_OP_REV_SUBTRACT;
            bld.RenderTarget[i].BlendOpAlpha =   D3D11_BLEND_OP_REV_SUBTRACT;
        }

        assert_hr(device->CreateBlendState(&bld, &bs_negative_inject_));

        cb_delta_injection_.create(device);
    }

    void delta_light_propagation_volume::set_direct_inject_shader(ID3D11VertexShader* vs)
    {
        exchange(&vs_direct_inject_, vs);
    }

    void delta_light_propagation_volume::destroy()
    {
        light_propagation_volume::destroy();

        safe_release(bs_negative_inject_);
        safe_release(vs_direct_inject_);

        cb_delta_injection_.destroy();
    }

    void delta_light_propagation_volume::render_direct(ID3D11DeviceContext* context, size_t num_iterations, UINT lpv_out_start_slot)
    {
        dune::set_viewport(context, volume_size_, volume_size_);

        profiler_.begin(context);
        normalize(context);
        time_normalize_ = profiler_.result();

        profiler_.begin(context);
        propagate_direct(context, num_iterations);
        time_propagate_ = profiler_.result();

        to_ps(context, lpv_out_start_slot);
    }

    void delta_light_propagation_volume::render_indirect(ID3D11DeviceContext* context)
    {
        dune::set_viewport(context, volume_size_, volume_size_);

        profiler_.begin(context);
        normalize(context);
        time_normalize_ = profiler_.result();

        profiler_.begin(context);
        propagate(context, iterations_rendered_);
        time_propagate_ = profiler_.result();
    }
}
