/*
* common_renderer.cpp by Tobias Alexander Franke (tob@cyberhead.de) 2014
* For copyright and license see LICENSE
* http://www.tobias-franke.eu
*/

#include "common_renderer.h"

#include <DirectXMath.h>

std::vector<dune::tstring>      files_scene;
float                           z_near = .2f;
float                           z_far = 100000.f;
dc::common_renderer*            the_renderer;
ID3D11Device*                   the_device = nullptr;
ID3D11DeviceContext*            the_context = nullptr;

namespace dc
{
    void common_renderer::create(ID3D11Device* device)
    {
        deferred_renderer::create(device);

        camera_.create(device);

        for (size_t i = 0; i < files_scene.size(); ++i)
            scene_.create_from_dir(device, files_scene[i].c_str());

        DirectX::XMMATRIX model = DirectX::XMMatrixIdentity();

        DirectX::XMFLOAT4X4 xmf_world;
        XMStoreFloat4x4(&xmf_world, model);
        
        scene_.set_world(xmf_world);
        debug_box_.set_world(xmf_world); 

        dune::load_texture(device, L"../../data/noise.dds", &noise_srv_);

        sky_.create(device, L"../../data/skydome/skydome_sphere.obj");
        sky_.set_envmap(device, L"../../data/skydome/sunny_day.jpg");

        cb_per_frame_.create(device);
        cb_onetime_.create(device);

#if defined(DEBUG) || defined(_DEBUG)
        dune::logger::show_warnings();
#endif

        def_.create(device, L"cam");

        auto pBackBufferSurfaceDesc = DXUTGetDXGIBackBufferSurfaceDesc();

        // setup postprocessor
        DXGI_SURFACE_DESC
            ld_color = *pBackBufferSurfaceDesc,
            ld_spec = *pBackBufferSurfaceDesc,
            ld_normal = *pBackBufferSurfaceDesc,
            ld_ldepth = *pBackBufferSurfaceDesc,
            ld_postpr = *pBackBufferSurfaceDesc;

        ld_color.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        ld_spec.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        ld_normal.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
        ld_ldepth.Format = DXGI_FORMAT_R32G32_FLOAT;
        ld_postpr.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

        // setup postprocessor
        // TODO: initialize gamma define if render target is not SRGB!
        postprocessor_.create(device, ld_postpr);

        // setup deferred camera gbuffer
        def_.add_target(L"colors", ld_color);
        def_.add_target(L"specular", ld_spec);
        def_.add_target(L"normals", ld_normal);
        def_.add_target(L"lineardepth", ld_ldepth, 0);

        add_buffer(def_, 2);

        set_postprocessor(&postprocessor_);
    }

    void common_renderer::set_shader(ID3D11Device* device,
        ID3DBlob* input_binary,
        ID3D11VertexShader* vs_deferred,
        ID3D11PixelShader* ps_deferred,
        ID3D11PixelShader* ps_overlay,
        UINT start_slot,
        UINT overlay_slot,
        UINT postprocessing_slot)
    {
        deferred_renderer::set_shader(device, input_binary, vs_deferred, ps_deferred, ps_overlay, start_slot, overlay_slot);
        postprocessor_.set_shader(device, input_binary, vs_deferred, postprocessing_slot);
    }

    void common_renderer::destroy()
    {
        deferred_renderer::destroy();

        postprocessor_.destroy();

        def_.destroy();

        camera_.destroy();
        debug_box_.destroy();
        sky_.destroy();
        scene_.destroy();
        dune::texture_cache::i().destroy();

        cb_onetime_.destroy();
        cb_per_frame_.destroy();
    }

    void common_renderer::resize(UINT width, UINT height)
    {
        deferred_renderer::resize(width, height);

        FLOAT aspect_ratio = static_cast<FLOAT>(width) / static_cast<FLOAT>(height);
        camera_.SetProjParams(static_cast<FLOAT>(DirectX::XM_PI / 4), aspect_ratio, z_near, z_far);

        def_.resize(width, height);
    }

    void common_renderer::update_postprocessing_parameters(ID3D11DeviceContext* context)
    {
        postprocessor_.parameters().to_ps(context, SLOT_POSTPROCESSING_PS);
    }

    void common_renderer::update_camera_parameters(ID3D11DeviceContext* context)
    {
        camera_.update(z_far);
        camera_.parameters().to_vs(context, SLOT_CAMERA_VS);
        camera_.parameters().to_ps(context, SLOT_CAMERA_PS);
    }

    // upload world coordinates of bounding box of mesh
    void common_renderer::update_bbox(ID3D11DeviceContext* context, dune::d3d_mesh& mesh)
    {
        DirectX::XMVECTOR v_bb_min = XMLoadFloat3(&mesh.bb_min());
        DirectX::XMVECTOR v_bb_max = XMLoadFloat3(&mesh.bb_max());

        DirectX::XMVECTOR v_diag = DirectX::XMVectorSubtract(v_bb_max, v_bb_min);

        DirectX::XMVECTOR delta = DirectX::XMVectorScale(v_diag, 0.05f);

        v_bb_min = DirectX::XMVectorSubtract(v_bb_min, delta);
        v_bb_max = DirectX::XMVectorAdd(v_bb_max, delta);

        v_bb_min = DirectX::XMVectorSetW(v_bb_min, 1.f);
        v_bb_max = DirectX::XMVectorSetW(v_bb_max, 1.f);

        auto world = DirectX::XMLoadFloat4x4(&mesh.world());
        v_bb_min = DirectX::XMVector4Transform(v_bb_min, world);
        v_bb_max = DirectX::XMVector4Transform(v_bb_max, world);

        DirectX::XMStoreFloat3(&bb_min_, v_bb_min);
        DirectX::XMStoreFloat3(&bb_max_, v_bb_max);

        auto cb = &cb_onetime_.data();
        {
            cb->scene_dim_max = DirectX::XMFLOAT4(bb_max_.x, bb_max_.y, bb_max_.z, 1.f);
            cb->scene_dim_min = DirectX::XMFLOAT4(bb_min_.x, bb_min_.y, bb_min_.z, 1.f);
        }
        cb_onetime_.to_ps(context, SLOT_ONETIME_PS);
        cb_onetime_.to_vs(context, SLOT_ONETIME_VS);
    }

    void common_renderer::update_onetime_parameters(ID3D11DeviceContext* context, dune::d3d_mesh& mesh)
    {
        update_bbox(context, mesh);
        context->PSSetShaderResources(SLOT_TEX_NOISE, 1, &noise_srv_);
    }

    // TODO: update_scene_parameters?
    void common_renderer::update_scene(ID3D11DeviceContext* context, const DirectX::XMMATRIX& model)
    {
        // cleanup
        DirectX::XMFLOAT4X4 xmf_world;
        XMStoreFloat4x4(&xmf_world, model);

        scene_.set_world(xmf_world);
        debug_box_.set_world(xmf_world);

        update_bbox(context, scene_);
    }

    // TODO: update_frame_parameters?
    void common_renderer::update_frame(ID3D11DeviceContext* context, double time, float elapsed_time)
    {
        auto cb = &cb_per_frame_.data();
        {
            cb->time = static_cast<float>(time);
            cb->time_delta = elapsed_time;
        }
        cb_per_frame_.to_ps(context, SLOT_PER_FRAME_PS);

        DirectX::XMVECTOR d = DirectX::XMVectorSubtract(
            XMLoadFloat3(&scene_.bb_max()), XMLoadFloat3(&scene_.bb_min()));

        FLOAT s;
        DirectX::XMStoreFloat(&s, DirectX::XMVector3Length(d));
        s /= 100.f;

        camera_.FrameMove(elapsed_time * s);
    }

    void common_renderer::update_camera(ID3D11DeviceContext* context, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        camera_.HandleMessages(hWnd, uMsg, wParam, lParam);
    }

    void common_renderer::render(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer, ID3D11DepthStencilView* dsv)
    {
        static float clear_color[4] = { 0.0f, 0.f, 0.0f, 1.0f };
        
        render_scene(context, clear_color, dsv);
        
        context->GenerateMips(def_[L"lineardepth"]->srv());

        // TODO: remove me
        dune::set_viewport(context, DXUTGetWindowWidth(), DXUTGetWindowHeight());

        deferred_renderer::render(context, backbuffer, dsv);
    }

    void common_renderer::render_scene(ID3D11DeviceContext* context, float* clear_color, ID3D11DepthStencilView* dsv)
    {
        update_camera_parameters(context);

        // render scene
        ID3D11RenderTargetView* def_views[] = {
            def_[L"colors"]->rtv(),
            def_[L"normals"]->rtv(),
            def_[L"lineardepth"]->rtv(),
            def_[L"specular"]->rtv()
        };

        DirectX::XMFLOAT2 size = def_[L"colors"]->size();
        dune::set_viewport(context,
            static_cast<size_t>(size.x),
            static_cast<size_t>(size.y));

        context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0, 0);
        dune::clear_rtvs(context, def_views, 4, clear_color);

        context->OMSetRenderTargets(4, def_views, dsv);

        do_render_scene(context);

        reset_omrtv(context);
    }

    void common_renderer::do_render_scene(ID3D11DeviceContext* context)
    {
        // FIXME: add proper depth map and render last!
        sky_.set_shader_slots(SLOT_TEX_DIFFUSE, -1, SLOT_TEX_SPECULAR);
        sky_.render(context);

        scene_.set_shader_slots(SLOT_TEX_DIFFUSE, SLOT_TEX_NORMAL, SLOT_TEX_SPECULAR);

        for (size_t x = 0; x < scene_.size(); ++x)
        {
            dune::gilga_mesh* m = dynamic_cast<dune::gilga_mesh*>(scene_[x].get());
            if (m) m->set_alpha_slot(SLOT_TEX_ALPHA);
        }

        scene_.render(context);
    }

    void common_renderer::reset_omrtv(ID3D11DeviceContext* context)
    {
        ID3D11RenderTargetView* null_views[] = { nullptr, nullptr, nullptr, nullptr };
        context->OMSetRenderTargets(4, null_views, nullptr);
    }
    
    void common_renderer::save(dune::serializer& s)
    {
        s << camera();
        s << postprocessor();
    }

    void common_renderer::load(ID3D11DeviceContext* context, const dune::serializer& s)
    {
        s >> camera();
        s >> postprocessor();
    }
}
