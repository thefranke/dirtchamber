/*
 * Dune D3D library - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "sparse_voxel_octree.h"

#include "d3d_tools.h"
#include "mesh.h"
#include "assimp_mesh.h"
#include "unicode.h"
#include "gbuffer.h"

namespace dune
{
    sparse_voxel_octree::sparse_voxel_octree() :
        bs_voxelize_(nullptr),
        ss_visualize_(nullptr),
        vs_voxelize_(nullptr),
        gs_voxelize_(nullptr),
        ps_voxelize_(nullptr),
        vs_inject_(nullptr),
        ps_inject_(nullptr),
        inject_rsm_rho_start_slot_(-1),
        v_normal_(nullptr),
        v_rho_(nullptr),
        srv_v_normal_(nullptr),
        srv_v_rho_(nullptr),
        uav_v_normal_(nullptr),
        uav_v_rho_(nullptr),
        no_culling_(nullptr),
        volume_size_(0),
        world_to_svo_(),
        svo_min_(),
        svo_max_(),
        cb_parameters_(),
        profiler_(),
        cb_gi_parameters_(),
        last_bound_(0),
        time_voxelize_(0),
        time_inject_(0),
        time_mip_(0)
    {
    }

    void sparse_voxel_octree::create(ID3D11Device* device, UINT volume_size)
    {
        profiler_.create(device);

        volume_size_ = volume_size;
        UINT voxel_count = volume_size_ * volume_size_ * volume_size_;
        UINT mips = static_cast<UINT>(std::log2(volume_size_));

        // create volume(s)
        D3D11_TEXTURE3D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
        desc.Width = volume_size;
        desc.Height = volume_size;
        desc.Depth = volume_size;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        desc.MipLevels = mips;

        assert_hr(device->CreateTexture3D(&desc, nullptr, &v_normal_));
        assert_hr(device->CreateTexture3D(&desc, nullptr, &v_rho_));

        // create unordered access view for volume(s)
        D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
        ZeroMemory(&uav_desc, sizeof(uav_desc));

        uav_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
        uav_desc.Texture3D.FirstWSlice = 0;
        uav_desc.Texture3D.MipSlice = 0;
        uav_desc.Texture3D.WSize = volume_size;

        assert_hr(device->CreateUnorderedAccessView(v_normal_, &uav_desc, &uav_v_normal_));
        assert_hr(device->CreateUnorderedAccessView(v_rho_, &uav_desc, &uav_v_rho_));

        // create shader resource view for volume(s)
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;

        srv_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        srv_desc.Texture3D.MostDetailedMip = 0;
        srv_desc.Texture3D.MipLevels = mips;

        assert_hr(device->CreateShaderResourceView(v_normal_, &srv_desc, &srv_v_normal_));
        assert_hr(device->CreateShaderResourceView(v_rho_, &srv_desc, &srv_v_rho_));

        D3D11_BLEND_DESC bld;
        ZeroMemory(&bld, sizeof(D3D11_BLEND_DESC));

        bld.IndependentBlendEnable = TRUE;

        for (size_t i = 0; i < 6; ++i)
        {
            bld.RenderTarget[i].BlendEnable = TRUE;
            bld.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
            bld.RenderTarget[i].DestBlend = D3D11_BLEND_ONE;
            bld.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
            bld.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ONE;
            bld.RenderTarget[i].BlendOp = D3D11_BLEND_OP_MAX;
            bld.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_MAX;
            bld.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        assert_hr(device->CreateBlendState(&bld, &bs_voxelize_));

        CD3D11_RASTERIZER_DESC raster_desc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
        raster_desc.FrontCounterClockwise = true;
        raster_desc.CullMode = D3D11_CULL_NONE;

        assert_hr(device->CreateRasterizerState(&raster_desc, &no_culling_));

        cb_parameters_.create(device);
        cb_gi_parameters_.create(device);

        DirectX::XMStoreFloat4x4(&world_to_svo_, DirectX::XMMatrixIdentity());
    }

    void sparse_voxel_octree::set_model_matrix(ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& model, const DirectX::XMFLOAT3& svo_min, const DirectX::XMFLOAT3& svo_max, UINT svo_parameters_slot)
    {
        DirectX::XMMATRIX m_model = DirectX::XMLoadFloat4x4(&model);

        DirectX::XMMATRIX model_inv = DirectX::XMMatrixInverse(nullptr, m_model);

        DirectX::XMVECTOR diag = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&svo_max), DirectX::XMLoadFloat3(&svo_min));

        DirectX::XMFLOAT3 d;
        DirectX::XMStoreFloat3(&d, diag);

        DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(1.f / d.x, 1.f / d.y, 1.f / d.z);

        DirectX::XMMATRIX trans = DirectX::XMMatrixTranslation(-svo_min.x, -svo_min.y, -svo_min.z);

        DirectX::XMMATRIX world_to_svo = model_inv * trans * scale;
        DirectX::XMStoreFloat4x4(&world_to_svo_, world_to_svo);

        svo_min_ = svo_min;
        svo_max_ = svo_max;

        DirectX::XMVECTOR v_svo_min = DirectX::XMLoadFloat3(&svo_min);
        DirectX::XMVECTOR v_svo_max = DirectX::XMLoadFloat3(&svo_max);

        v_svo_min = DirectX::XMVector3Transform(v_svo_min, m_model);
        v_svo_max = DirectX::XMVector3Transform(v_svo_max, m_model);

        auto cb = &cb_parameters_.data();
        {
            DirectX::XMStoreFloat4x4(&cb->world_to_svo,
                world_to_svo);

            DirectX::XMStoreFloat4(&cb->bb_min, v_svo_min);
            DirectX::XMStoreFloat4(&cb->bb_max, v_svo_max);
        }
        cb_parameters_.to_vs(context, svo_parameters_slot);
        cb_parameters_.to_gs(context, svo_parameters_slot);
        cb_parameters_.to_ps(context, svo_parameters_slot);
    }

    void sparse_voxel_octree::destroy()
    {
        profiler_.destroy();

        cb_parameters_.destroy();
        cb_gi_parameters_.destroy();

        safe_release(bs_voxelize_);
        safe_release(ss_visualize_);

        safe_release(vs_voxelize_);
        safe_release(gs_voxelize_);
        safe_release(ps_voxelize_);

        safe_release(vs_inject_);
        safe_release(ps_inject_);

        safe_release(uav_v_normal_);
        safe_release(uav_v_rho_);

        safe_release(srv_v_normal_);
        safe_release(srv_v_rho_);

        safe_release(v_normal_);
        safe_release(v_rho_);

        safe_release(no_culling_);

        inject_rsm_rho_start_slot_ = -1;
        last_bound_ = 0;
        volume_size_ = 0;
    }

    void sparse_voxel_octree::inject(ID3D11DeviceContext* context, directional_light& light)
    {
        profiler_.begin(context);

        float clear_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };

        // TODO
        //context->ClearUnorderedAccessViewFloat(uav_v_rho_, clear_color);

        ID3D11UnorderedAccessView* uav_views[] = { uav_v_rho_ };

        context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 1, 1, uav_views, nullptr);

        light.rsm().to_ps(context, inject_rsm_rho_start_slot_);

        DirectX::XMFLOAT2 size = light.rsm().targets()[0].size();

        UINT num_vpls = static_cast<UINT>(size.x * size.y);

        context->VSSetShader(vs_inject_, nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(ps_inject_, nullptr, 0);

        // create pseudo buffer and call shader for each pixel in the rsm
        unsigned int stride = 0;
        unsigned int offset = 0;
        ID3D11Buffer* buffer[] = { nullptr };

        context->IASetVertexBuffers(0, 1, buffer, &stride, &offset);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

        context->Draw(num_vpls, 0);

        // cleanup
        ID3D11ShaderResourceView* clear_srvs[] = { nullptr, nullptr };
        context->PSSetShaderResources(inject_rsm_rho_start_slot_, 1, clear_srvs);

        ID3D11UnorderedAccessView* clear_uavs[] = { nullptr };
        context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 1, 1, clear_uavs, nullptr);

        time_inject_ = profiler_.result();
    }

    void sparse_voxel_octree::filter(ID3D11DeviceContext* context)
    {
        profiler_.begin(context);

        context->GenerateMips(srv_v_normal_);
        context->GenerateMips(srv_v_rho_);

        time_mip_ = profiler_.result();
    }

    void sparse_voxel_octree::clear_ps(ID3D11DeviceContext* context)
    {
        ID3D11ShaderResourceView* srv_null[] = { nullptr, nullptr };
        context->PSSetShaderResources(last_bound_, 2, srv_null);
    }

    void sparse_voxel_octree::to_ps(ID3D11DeviceContext* context, UINT volume_start_slot)
    {
        UINT normal_slot = volume_start_slot + 0,
            rho_slot = volume_start_slot + 1;

        last_bound_ = volume_start_slot;

        context->PSSetShaderResources(normal_slot, 1, &srv_v_normal_);
        context->PSSetShaderResources(rho_slot, 1, &srv_v_rho_);
    }

    void sparse_voxel_octree::set_shader_voxelize(ID3D11VertexShader* vs, ID3D11GeometryShader* gs, ID3D11PixelShader* ps)
    {
        exchange(&vs_voxelize_, vs);
        exchange(&ps_voxelize_, ps);
        exchange(&gs_voxelize_, gs);
    }

    void sparse_voxel_octree::set_shader_inject(ID3D11VertexShader* vs, ID3D11PixelShader* ps, UINT rsm_start_slot)
    {
        exchange(&vs_inject_, vs);
        exchange(&ps_inject_, ps);

        inject_rsm_rho_start_slot_ = rsm_start_slot;
    }

    void sparse_voxel_octree::voxelize(ID3D11DeviceContext* context, gilga_mesh& mesh, bool clear)
    {
        clear_ps(context);

        profiler_.begin(context);

        dune::set_viewport(context, volume_size_, volume_size_);

        context->RSSetState(no_culling_);

        if (clear)
        {
            float clear[4] = { 0.f, 0.f, 0.f, 0.f };
            context->ClearUnorderedAccessViewFloat(uav_v_normal_, clear);
            context->ClearUnorderedAccessViewFloat(uav_v_rho_, clear);
        }

        ID3D11UnorderedAccessView* uavs[2] = { uav_v_normal_, uav_v_rho_ };

        context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 1, 2, uavs, nullptr);

        FLOAT factors[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        context->OMSetBlendState(bs_voxelize_, factors, 0xffffffff);

        context->VSSetShader(vs_voxelize_, nullptr, 0);
        context->GSSetShader(gs_voxelize_, nullptr, 0);
        context->PSSetShader(ps_voxelize_, nullptr, 0);

        // override mesh so it doesn't select it's own shaders
        mesh.render_direct(context, nullptr);

        // clear and done
        ID3D11UnorderedAccessView* clear_uavs[] = { nullptr, nullptr };
        context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 1, 2, clear_uavs, nullptr);

        time_voxelize_ = profiler_.result();
    }

    delta_sparse_voxel_octree::delta_sparse_voxel_octree() :
        v_delta_(nullptr),
        srv_v_delta_(nullptr),
        uav_v_delta_(nullptr),
        inject_rsm_mu_start_slot_(-1)
    {
    }

    void delta_sparse_voxel_octree::set_shader_inject(ID3D11VertexShader* vs, ID3D11PixelShader* ps, UINT rsm_mu_start_slot, UINT rsm_rho_start_slot)
    {
        exchange(&vs_inject_, vs);
        exchange(&ps_inject_, ps);

        inject_rsm_mu_start_slot_ = rsm_mu_start_slot;
        inject_rsm_rho_start_slot_ = rsm_rho_start_slot;
    }

    void delta_sparse_voxel_octree::inject(ID3D11DeviceContext* context, differential_directional_light& light)
    {
        profiler_.begin(context);

        float clear_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };

        context->ClearUnorderedAccessViewFloat(uav_v_delta_, clear_color);
        context->ClearUnorderedAccessViewFloat(uav_v_rho_, clear_color);

        ID3D11UnorderedAccessView* uav_views[] = { uav_v_rho_, uav_v_delta_ };

        context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 1, 2, uav_views, nullptr);

        light.mu().to_ps(context, inject_rsm_mu_start_slot_);
        light.rho().to_ps(context, inject_rsm_rho_start_slot_);

        DirectX::XMFLOAT2 size = light.mu().targets()[0].size();

        UINT num_vpls = static_cast<UINT>(size.x * size.y);

        context->VSSetShader(vs_inject_, nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(ps_inject_, nullptr, 0);

        // create pseudo buffer and call shader for each pixel in the rsm
        unsigned int stride = 0;
        unsigned int offset = 0;
        ID3D11Buffer* buffer[] = { nullptr };

        context->IASetVertexBuffers(0, 1, buffer, &stride, &offset);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

        context->Draw(num_vpls, 0);

        // cleanup
        ID3D11ShaderResourceView* clear_srvs[] = { nullptr, nullptr, nullptr };
        context->PSSetShaderResources(inject_rsm_rho_start_slot_, 3, clear_srvs);
        context->PSSetShaderResources(inject_rsm_mu_start_slot_, 3, clear_srvs);

        ID3D11UnorderedAccessView* clear_uavs[] = { nullptr, nullptr };
        context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 1, 2, clear_uavs, nullptr);

        time_inject_ = profiler_.result();
    }

    void delta_sparse_voxel_octree::clear_ps(ID3D11DeviceContext* context)
    {
        ID3D11ShaderResourceView* srv_null[] = { nullptr, nullptr, nullptr };
        context->PSSetShaderResources(last_bound_, 3, srv_null);
    }

    void delta_sparse_voxel_octree::to_ps(ID3D11DeviceContext* context, UINT volume_start_slot)
    {
        UINT normal_slot = volume_start_slot + 0,
             rho_slot = volume_start_slot + 1,
             delta_slot = volume_start_slot + 2;

        last_bound_ = volume_start_slot;

        profiler_.begin(context);

        context->GenerateMips(srv_v_normal_);
        context->PSSetShaderResources(normal_slot, 1, &srv_v_normal_);

        context->GenerateMips(srv_v_rho_);
        context->PSSetShaderResources(rho_slot, 1, &srv_v_rho_);

        context->GenerateMips(srv_v_delta_);
        context->PSSetShaderResources(delta_slot, 1, &srv_v_delta_);

        time_mip_ = profiler_.result();
    }

    void delta_sparse_voxel_octree::create(ID3D11Device* device, UINT volume_size)
    {
        // create regular stuff first
        sparse_voxel_octree::create(device, volume_size);

        // create delta volume
        D3D11_TEXTURE3D_DESC desc;
        v_rho_->GetDesc(&desc);
        assert_hr(device->CreateTexture3D(&desc, nullptr, &v_delta_));

        // create delta uav
        D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
        uav_v_rho_->GetDesc(&uav_desc);
        assert_hr(device->CreateUnorderedAccessView(v_delta_, &uav_desc, &uav_v_delta_));

        // create delta srv
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
        srv_v_rho_->GetDesc(&srv_desc);
        assert_hr(device->CreateShaderResourceView(v_delta_, &srv_desc, &srv_v_delta_));
    }

    void delta_sparse_voxel_octree::destroy()
    {
        // destroy other stuff first
        sparse_voxel_octree::destroy();

        safe_release(uav_v_delta_);
        safe_release(srv_v_delta_);
        safe_release(v_delta_);

        inject_rsm_mu_start_slot_ = -1;
    }
}
