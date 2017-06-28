/*
 * The Dirtchamber - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#include <dune/dune.h>

#include "common_renderer.h"
#include "common_dxut.h"
#include "common_gui.h"

/*!
 * \brief A Delta Radiance Field renderer for augmented reality.
 *
 * This class is an implementation of an augmented reality relighting renderer using
 * Delta Radiance Fields (see README.md for more information on publications). Depending
 * on the define \#DLPV, this class will use different volumetric GI methods to
 * relight the surrounding scene: if \#DLPV is set, Delta Light Propagation Volumes
 * will be used for GI. If \#DLPV isn't set, Delta Voxel Cone Tracing will do the job.
 *
 * A Kinect camera is required to run this sample as a *true* AR renderer. A marker
 * to print out is available in /data/marker.png.
 *
 * If you do not have a Kinect or haven't compiled Dune with support for the Microsoft
 * Kinect SDK (i.e \#MICROSOFT_KINECT_SDK isn't defined), you can still use the sample
 * with a static background (see /data/real_static_scene.png), which is a
 * snapshot frame of a Kinect color image.
 */
class drf_renderer : public dc::rsm_renderer <dune::differential_directional_light>
{
public:
    float time_rsm_mu_;
    float time_rsm_rho_;
    float time_deferred_;

protected:

#ifdef DLPV
    dune::delta_light_propagation_volume delta_radiance_field_;
    dune::light_propagation_volume lpv_rho_;
    #define VOLUME_SIZE LPV_SIZE
    #define VOLUME_PARAMETERS_SLOT SLOT_LPV_PARAMETERS_VS_PS
#else
    dune::delta_sparse_voxel_octree delta_radiance_field_;
    #define VOLUME_SIZE SVO_SIZE
    #define VOLUME_PARAMETERS_SLOT SLOT_SVO_PARAMETERS_VS_GS_PS
#endif

#ifdef MICROSOFT_KINECT_SDK
    dune::kinect_gbuffer kinect_;
#else
    dune::render_target real_static_background_;
#endif

    dune::tracker tracker_;
    dune::profile_query profiler_;
    dune::gilga_mesh synthetic_object_;
    #define reconstructed_real_scene_ scene_

    ID3D11RasterizerState* no_culling_;

    bool render_mu_;
    float rotation_angle_;

protected:
    void load_shader(ID3D11Device* device)
    {
        // setup mesh effect
        DWORD shader_flags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(DEBUG) | defined(_DEBUG)
        shader_flags |= D3DCOMPILE_DEBUG;
#endif

        ID3DBlob* vs_blob = nullptr;

        ID3D11VertexShader* vs = nullptr;
        ID3D11VertexShader* vs1 = nullptr;
        ID3D11VertexShader* vs2 = nullptr;
        ID3D11GeometryShader* gs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        ID3D11PixelShader* ps1 = nullptr;
        ID3D11PixelShader* ps2 = nullptr;

        auto cleanup = [&](){ dune::safe_release(vs_blob); dune::safe_release(vs); dune::safe_release(vs1); dune::safe_release(vs2); dune::safe_release(ps); dune::safe_release(ps1); dune::safe_release(ps2); dune::safe_release(gs); };

        // mesh shader
        dune::compile_shader(device, L"../../shader/d3d_mesh.hlsl", "vs_5_0", "vs_mesh", shader_flags, nullptr, &vs, &vs_blob);
        dune::compile_shader(device, L"../../shader/d3d_mesh.hlsl", "ps_5_0", "ps_mesh", shader_flags, nullptr, &ps);
        reconstructed_real_scene_.set_shader(device, vs_blob, vs, ps);
        synthetic_object_.set_shader(device, vs_blob, vs, ps);
        cleanup();

        // deferred shader + postprocessor
        dune::compile_shader(device, L"../../shader/fs_triangle.hlsl",          "vs_5_0", "vs_fs_triangle",     shader_flags, nullptr, &vs, &vs_blob);
#ifdef DLPV
        dune::compile_shader(device, L"../../shader/deferred_dlpv_kinect.hlsl", "ps_5_0", "ps_ar_dlpv", shader_flags, nullptr, &ps);
#else
        dune::compile_shader(device, L"../../shader/deferred_dvct_kinect.hlsl", "ps_5_0", "ps_ar_dvct", shader_flags, nullptr, &ps);
#endif
        dune::compile_shader(device, L"../../shader/overlay.hlsl",              "ps_5_0", "ps_overlay",         shader_flags, nullptr, &ps1);
        set_shader(device, vs_blob, vs, ps, ps1, SLOT_TEX_DEFERRED_START, SLOT_TEX_OVERLAY, SLOT_TEX_POSTPROCESSING_START);
        cleanup();

        // skydome shader
        dune::compile_shader(device, L"../../shader/d3d_mesh_skydome.hlsl", "vs_5_0", "vs_skydome", shader_flags, nullptr, &vs, &vs_blob);
        dune::compile_shader(device, L"../../shader/d3d_mesh_skydome.hlsl", "ps_5_0", "ps_skydome", shader_flags, nullptr, &ps);
        sky_.set_shader(device, vs_blob, vs, ps);
        cleanup();

#ifdef DLPV
        // lpv inject shader
        dune::compile_shader(the_device, L"../../shader/lpv_inject.hlsl", "vs_5_0", "vs_delta_lpv_inject", shader_flags, nullptr, &vs);
        dune::compile_shader(the_device, L"../../shader/lpv_inject.hlsl", "vs_5_0", "vs_lpv_inject", shader_flags, nullptr, &vs1);
        dune::compile_shader(the_device, L"../../shader/lpv_inject.hlsl", "gs_5_0", "gs_lpv_inject", shader_flags, nullptr, &gs);
        dune::compile_shader(the_device, L"../../shader/lpv_inject.hlsl", "ps_5_0", "ps_delta_lpv_inject", shader_flags, nullptr, &ps);
        dune::compile_shader(the_device, L"../../shader/lpv_inject.hlsl", "ps_5_0", "ps_lpv_inject", shader_flags, nullptr, &ps2);
        dune::compile_shader(the_device, L"../../shader/lpv_normalize.hlsl", "ps_5_0", "ps_lpv_normalize", shader_flags, nullptr, &ps1);
        dune::compile_shader(the_device, L"../../shader/lpv_inject.hlsl", "vs_5_0", "vs_delta_lpv_direct_inject", shader_flags, nullptr, &vs2);
        delta_radiance_field_.set_inject_shader(vs, gs, ps, ps1, SLOT_TEX_LPV_INJECT_RSM_START);
        delta_radiance_field_.set_direct_inject_shader(vs2);
        lpv_rho_.set_inject_shader(vs1, gs, ps2, ps1, SLOT_TEX_LPV_INJECT_RSM_START);
        cleanup();

        // lpv propagate shader
        dune::compile_shader(the_device, L"../../shader/lpv_propagate.hlsl", "vs_5_0", "vs_lpv_propagate", shader_flags, nullptr, &vs, &vs_blob);
        dune::compile_shader(the_device, L"../../shader/lpv_propagate.hlsl", "gs_5_0", "gs_lpv_propagate", shader_flags, nullptr, &gs);
        dune::compile_shader(the_device, L"../../shader/lpv_propagate.hlsl", "ps_5_0", "ps_delta_lpv_propagate", shader_flags, nullptr, &ps);
        dune::compile_shader(the_device, L"../../shader/lpv_propagate.hlsl", "ps_5_0", "ps_lpv_propagate", shader_flags, nullptr, &ps1);
        delta_radiance_field_.set_propagate_shader(the_device, vs, gs, ps, vs_blob, SLOT_TEX_LPV_PROPAGATE_START);
        lpv_rho_.set_propagate_shader(the_device, vs, gs, ps1, vs_blob, SLOT_TEX_LPV_PROPAGATE_START);
        cleanup();

        // lpv volume visualization
        dune::compile_shader(the_device, L"../../shader/lpv_rendervol.hlsl", "vs_5_0", "vs_rendervol", shader_flags, nullptr, &vs, &vs_blob);
        dune::compile_shader(the_device, L"../../shader/lpv_rendervol.hlsl", "ps_5_0", "ps_rendervol", shader_flags, nullptr, &ps);
        debug_box_.set_shader(the_device, vs_blob, vs, ps);
        cleanup();
#else
        // svo voxelize shader
        dune::compile_shader(device, L"../../shader/svo_voxelize.hlsl", "vs_5_0", "vs_svo_voxelize", shader_flags, nullptr, &vs);
        dune::compile_shader(device, L"../../shader/svo_voxelize.hlsl", "gs_5_0", "gs_svo_voxelize", shader_flags, nullptr, &gs);
        dune::compile_shader(device, L"../../shader/svo_voxelize.hlsl", "ps_5_0", "ps_svo_voxelize", shader_flags, nullptr, &ps);
        delta_radiance_field_.set_shader_voxelize(vs, gs, ps);
        cleanup();

        // svo inject shader
        dune::compile_shader(device, L"../../shader/svo_inject.hlsl", "vs_5_0", "vs_svo_inject", shader_flags, nullptr, &vs);
        dune::compile_shader(device, L"../../shader/svo_inject.hlsl", "ps_5_0", "ps_svo_delta_inject", shader_flags, nullptr, &ps);
        delta_radiance_field_.set_shader_inject(vs, ps, SLOT_TEX_SVO_RSM_MU_START, SLOT_TEX_SVO_RSM_RHO_START);
        cleanup();
#endif
    }

    /*! \brief Renders a Delta Radiance Field. */
    void render_drf(ID3D11DeviceContext* context, float* clear_color)
    {
#ifdef DLPV
        ID3D11ShaderResourceView* null_srv[] = { nullptr, nullptr, nullptr };
        context->PSSetShaderResources(SLOT_TEX_LPV_DEFERRED_START, 3, null_srv);

        lpv_rho_.inject(context, main_light_.rho(), true);
        lpv_rho_.render(context);

        float vpl_scale  = delta_radiance_field_.parameters().data().vpl_scale;

        // TODO: fixme
        float flux_scale = main_light_.parameters().data().flux.x;

        // inject indirect
        delta_radiance_field_.inject(context, main_light_.rho(), true, false, vpl_scale);
        delta_radiance_field_.inject(context, main_light_.mu(), false, false, vpl_scale);

        delta_radiance_field_.render_indirect(context);

        // inject direct
        delta_radiance_field_.inject(context, main_light_.rho(), true, true, flux_scale);
        delta_radiance_field_.inject(context, main_light_.mu(), false, true, flux_scale);

        delta_radiance_field_.render_direct(context, 5, SLOT_TEX_LPV_DEFERRED_START);
#else
        // TODO: voxelize only on geo change
        // voxelize reconstructed real scene
        for (size_t x = 0; x < reconstructed_real_scene_.size(); ++x)
        {
            dune::gilga_mesh* m = dynamic_cast<dune::gilga_mesh*>(reconstructed_real_scene_[x].get());

            if (m)
                delta_radiance_field_.voxelize(context, *m);
        }

        // voxelize synthetic object
        delta_radiance_field_.voxelize(context, synthetic_object_);

        // inject differential light, i.e. RSM rho and mu
        delta_radiance_field_.inject(context, main_light_);

        // prefilter volume
        delta_radiance_field_.filter(context);

        // upload to gpu
        delta_radiance_field_.to_ps(context, SLOT_TEX_SVO_V_START);
#endif
    }

    /*!
     * \brief Compute global illumination for the scene.
     *
     * This method first makes sure that the RSM/GI solution needs to be updated.
     * If so, a new RSM is rendered for one directional light, which will then be injected
     * into a volume and further processed.
     */
    void render_gi(ID3D11DeviceContext* context, float* clear_color)
    {
        if (update_rsm_)
        {
            // setup rsm view
            update_rsm_camera_parameters(context, main_light_);

            // inject first bounce
            render_mu_ = false;
            profiler_.begin(context);
            render_rsm(context, clear_color, main_light_.rho());
            time_rsm_rho_ = profiler_.result();

            render_mu_ = true;
            profiler_.begin(context);
            render_rsm(context, clear_color, main_light_.mu());
            time_rsm_mu_ = profiler_.result();

            // render delta radiance field
            render_drf(context, clear_color);

            update_rsm_ = false;
        }
    }

    virtual void do_render_scene(ID3D11DeviceContext* context)
    {
        // play safe just in case real scene is just a quad
        context->RSSetState(no_culling_);

        reconstructed_real_scene_.set_shader_slots(SLOT_TEX_DIFFUSE, SLOT_TEX_NORMAL, SLOT_TEX_SPECULAR);
        synthetic_object_.set_shader_slots(SLOT_TEX_DIFFUSE, SLOT_TEX_NORMAL, SLOT_TEX_SPECULAR);

        reconstructed_real_scene_.render(context);
        synthetic_object_.render(context);
    }

    virtual void do_render_rsm(ID3D11DeviceContext* context)
    {
        // play safe just in case real scene is just a quad
        context->RSSetState(no_culling_);

        reconstructed_real_scene_.set_shader_slots(SLOT_TEX_DIFFUSE, SLOT_TEX_NORMAL);
        synthetic_object_.set_shader_slots(SLOT_TEX_DIFFUSE, SLOT_TEX_NORMAL);

        reconstructed_real_scene_.render(context);

        if (!render_mu_)
            synthetic_object_.render(context);
    }

public:
    virtual void create(ID3D11Device* device)
    {
        // remove last entry from files_scene and use as synthetic object
        dune::tstring so_file = *files_scene.rbegin();
        files_scene.pop_back();

        rsm_renderer::create(device);

        profiler_.create(device);
        tracker_.create(L"../../data/camera_parameters.xml");
        tracker_.load_pattern(L"../../data/marker.png");

        synthetic_object_.create(device, so_file);

        delta_radiance_field_.create(device, VOLUME_SIZE);

#ifdef DLPV
        lpv_rho_.create(device, VOLUME_SIZE);
#endif

        CD3D11_RASTERIZER_DESC raster_desc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
        raster_desc.FrontCounterClockwise = true;
        raster_desc.CullMode = D3D11_CULL_NONE;

        dune::assert_hr(device->CreateRasterizerState(&raster_desc, &no_culling_));

        load_shader(device);

#ifdef MICROSOFT_KINECT_SDK
        kinect_.create(device, L"kinect");
        add_buffer(kinect_, SLOT_TEX_DEFERRED_KINECT_START);
        kinect_.start();
#else
        dune::load_static_target(device, L"../../data/real_static_scene.png", real_static_background_);
        real_static_background_.name = L"static_rgb";
        add_buffer(real_static_background_, SLOT_TEX_DEFERRED_KINECT_START);
#endif
    }

    virtual void destroy()
    {
#ifdef MICROSOFT_KINECT_SDK
        kinect_.destroy();
#else
        real_static_background_.destroy();
#endif

        rsm_renderer::destroy();
        delta_radiance_field_.destroy();
        synthetic_object_.destroy();
        dune::safe_release(no_culling_);
        profiler_.destroy();

#ifdef DLPV
        lpv_rho_.destroy();
#endif
    }

    virtual void render(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer, ID3D11DepthStencilView* dsv)
    {
        static float clear_color[4] = { 0.0f, 0.f, 0.0f, 1.0f };

#ifdef MICROSOFT_KINECT_SDK
        kinect_.update(context);
        tracker_.track_frame(*kinect_.color());
#else
        tracker_.track_frame(real_static_background_);
#endif

        update_tracked_scene(context);

        render_gi(context, clear_color);
        render_scene(context, clear_color, dsv);

        context->GenerateMips(main_light_.rho()[L"lineardepth"]->srv());
        context->GenerateMips(main_light_.mu()[L"lineardepth"]->srv());
        context->GenerateMips(def_[L"lineardepth"]->srv());

        dune::set_viewport(context, DXUTGetWindowWidth(), DXUTGetWindowHeight());

        profiler_.begin(context);
        deferred_renderer::render(context, backbuffer, dsv);
        time_deferred_ = profiler_.result();
    }

    void update_gi_parameters(ID3D11DeviceContext* context)
    {
        delta_radiance_field_.set_model_matrix(context, synthetic_object_.world(), bb_min_, bb_max_, VOLUME_PARAMETERS_SLOT);
        delta_radiance_field_.parameters().to_ps(context, SLOT_GI_PARAMETERS_PS);

#ifdef DLPV
        lpv_rho_.set_model_matrix(context, synthetic_object_.world(), bb_min_, bb_max_, VOLUME_PARAMETERS_SLOT);
        lpv_rho_.parameters().to_ps(context, SLOT_GI_PARAMETERS_PS);
#endif

        update_rsm_ = true;
    }

    /*! \brief Update all objects (synthetic and real reconstructed scene) with a matrix from the tracker. */
    void update_tracked_scene(ID3D11DeviceContext* context)
    {
        using namespace DirectX;

        XMMATRIX empty = XMMatrixIdentity();
        XMFLOAT4X4 tempty;
        XMStoreFloat4x4(&tempty, empty);

        XMFLOAT3 etrans = { 0, 0, -2.5 };

        // update virtual object now
        update_bbox(context, synthetic_object_);

        XMVECTOR trans =
        {
            dc::gui::slider_value(IDC_TRACK_TRANSX, true) * 2.0f - 1.0f,
            dc::gui::slider_value(IDC_TRACK_TRANSY, true) * 2.0f - 1.0f,
            dc::gui::slider_value(IDC_TRACK_TRANSZ, true) * 2.0f - 1.0f,
            0.f
        };

        trans = XMVectorScale(trans, 100);

        float scale = dc::gui::slider_value(IDC_TRACK_SCALE, true) * 100;

        XMFLOAT3 ttrans;
        XMStoreFloat3(&ttrans, trans);

        XMFLOAT4X4 tmodel = tracker_.model_view_matrix(scale, ttrans, tempty, 0);

        // set virtual object model view matrix
        synthetic_object_.set_world(tmodel);

        // update synthetic objects
        XMMATRIX rot = XMMatrixIdentity();

        if (dc::gui::checkbox_value(IDC_TRACK_ANIMATE))
            rot *= XMMatrixRotationY(rotation_angle_);

        update_scene(context, rot * DirectX::XMLoadFloat4x4(&tmodel));

        // calculate new bounds 2x the size of the maximum edge of the virtual objects bounding box
        XMFLOAT3 bb_min = synthetic_object_.bb_min();
        XMFLOAT3 bb_max = synthetic_object_.bb_max();

        XMVECTOR v_bb_min = XMLoadFloat3(&bb_min);
        XMVECTOR v_bb_max = XMLoadFloat3(&bb_max);

        XMVECTOR v_bb_diag = XMVectorSubtract(v_bb_max, v_bb_min);

        float l = std::max(std::max(XMVectorGetX(v_bb_diag), XMVectorGetY(v_bb_diag)), XMVectorGetZ(v_bb_diag));
        l *= 2;

        XMVECTORF32 l_edge = { l, l, l, 0 };

        XMVECTOR v_bb_center = XMVectorScale(XMVectorAdd(v_bb_max, v_bb_min), 0.5f);

        v_bb_max = v_bb_center + l_edge;
        v_bb_min = v_bb_center - l_edge;

        XMStoreFloat3(&bb_min_, v_bb_min);
        XMStoreFloat3(&bb_max_, v_bb_max);

        dc::gui::get_parameters(main_light_, synthetic_object_, z_near, z_far);
        update_light_parameters(context, main_light_);
        update_gi_parameters(context);
    }

    void update_frame(ID3D11DeviceContext* context, double time, float elapsed_time)
    {
        rsm_renderer::update_frame(context, time, elapsed_time);
        rotation_angle_ += 0.5f * elapsed_time;
    }

    void update_everything(ID3D11DeviceContext* context)
    {
        update_tracked_scene(context);
        update_onetime_parameters(context, synthetic_object_);
        update_camera_parameters(context);
        update_light_parameters(context, main_light_);
        update_postprocessing_parameters(context);
        update_gi_parameters(context);
    }

    void reload_shader(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        load_shader(device);
        update_everything(context);
    }

    virtual void save(dune::serializer& s)
    {
        rsm_renderer::save(s);
        s << delta_radiance_field_;
    }

    virtual void load(ID3D11DeviceContext* context, const dune::serializer& s)
    {
        rsm_renderer::load(context, s);

        s >> delta_radiance_field_;

        update_everything(context);
    }

    inline auto drf() -> decltype((delta_radiance_field_))
    {
        return delta_radiance_field_;
    }

#ifdef DLPV
    inline dune::light_propagation_volume& lpv_rho()
    {
        return lpv_rho_;
    }
#endif

    inline dune::gilga_mesh& synthetic_object()
    {
        return synthetic_object_;
    }

    inline dune::tracker& tracker()
    {
        return tracker_;
    }
};

drf_renderer renderer;

// gui
CDXUTDialog                             hud_mr;
float                                   rotation_angle = 0.0;

void update_timings_gui()
{
    dune::tstringstream ss;

    float time_sum = 0;

    ss  << L"RSM mu: " << renderer.time_rsm_mu_ << L"ms\n"
        << L"RSM rho: " << renderer.time_rsm_rho_ << L"ms\n";

    time_sum += renderer.time_rsm_mu_ + renderer.time_rsm_rho_;

#ifdef DLPV
    ss  << L"Inject: " << renderer.drf().time_inject_ << L"ms\n"
        << L"Normalize: " << renderer.drf().time_normalize_ << L"ms\n"
        << L"Propagate: " << renderer.drf().time_propagate_ << L"ms\n";

    time_sum += renderer.drf().time_inject_ + renderer.drf().time_normalize_ + renderer.drf().time_propagate_;
#else
    ss  << L"Voxelize: " << renderer.drf().time_voxelize_ << L"ms\n"
        << L"Inject: " << renderer.drf().time_inject_ << L"ms\n"
        << L"Filtering: " << renderer.drf().time_mip_ << L"ms\n";

    time_sum += renderer.drf().time_voxelize_ + renderer.drf().time_inject_ + renderer.drf().time_mip_;
#endif

    ss  << L"Deferred: " << renderer.time_deferred_ << L"ms\n"
        << L"Tracking: " << renderer.tracker().time_track_ << L"ms \n"
        << L"Sum Render:" << time_sum + renderer.time_deferred_ << L"ms\n";

    dc::gui::set_text(IDC_DEBUG_INFO + 0, ss.str().c_str());
}

void CALLBACK on_keyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
{
    dc::on_keyboard(nChar, bKeyDown, bAltDown, pUserContext);

    static const auto load_stuff = [](dune::serializer& s){ renderer.load(the_context, s); };

    if (nChar == 'L' && bKeyDown)
    {
        if (dc::gui::load_settings(load_stuff))
        {
            dc::gui::set_parameters(renderer.postprocessor());
            dc::gui::set_parameters(renderer.drf());
            dc::gui::set_parameters(renderer.main_light(), renderer.scene(), z_near, z_far);
        }
    }
}

HRESULT CALLBACK on_create_device(ID3D11Device* device, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
    tclog << L"State: Create" << std::endl;

    the_device = device;
    the_context = DXUTGetD3D11DeviceContext();

    dune::assert_hr(dc::gui::dlg_manager.OnD3D11CreateDevice(device, the_context));

    renderer.create(device);
    the_renderer = &renderer;

    dc::gui::get_parameters(renderer.drf());
#ifdef DLPV
    dc::gui::get_parameters(renderer.lpv_rho());
#endif
    dc::gui::get_parameters(renderer.postprocessor());
    dc::gui::get_parameters(renderer.main_light(), renderer.synthetic_object(), z_near, z_far);

    renderer.update_everything(the_context);

    return S_OK;
}

void CALLBACK on_render(ID3D11Device* device, ID3D11DeviceContext* context, double fTime, float fElapsedTime, void* pUserContext)
{
    dc::on_render(device, context, fTime, fElapsedTime, pUserContext);

    update_timings_gui();
}

void CALLBACK on_gui_event(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
    dc::on_gui_event(nEvent, nControlID, pControl, pUserContext);

    CDXUTDialog* current_tab = pControl->m_pDialog;

    if (current_tab == &dc::gui::hud_gi)
    {
        dc::gui::get_parameters(renderer.drf());
#ifdef DLPV
        dc::gui::get_parameters(renderer.lpv_rho());
#endif
        renderer.update_gi_parameters(the_context);
    }

    if (current_tab == &dc::gui::hud_light)
    {
        dc::gui::get_parameters(renderer.main_light(), renderer.scene(), z_near, z_far);
        renderer.update_light_parameters(the_context, renderer.main_light());
    }
}

void init_gui()
{
    int dd = 18, db = dd * 2;
    int x = 0, y = 0, w = 160, h = 22;

    // init default gui
    int start = dc::gui::init(on_gui_event, dd, w, h);

    hud_mr.Init(&dc::gui::dlg_manager);
    hud_mr.SetCallback(on_gui_event);

    y = start;

    dc::gui::combo_settings->AddItem(L"Tracking", reinterpret_cast<void*>(&hud_mr));

    hud_mr.AddStatic(-1, L"Tracked object X/Y/Z:", x, y += db, w, h);
    hud_mr.AddSlider(IDC_TRACK_TRANSX, x, y += dd, w, h);
    hud_mr.AddSlider(IDC_TRACK_TRANSY, x, y += dd, w, h);
    hud_mr.AddSlider(IDC_TRACK_TRANSZ, x, y += dd, w, h);

    hud_mr.AddStatic(-1, L"Tracked object scale:", x, y += db, w, h);
    hud_mr.AddSlider(IDC_TRACK_SCALE, x, y += dd, w, h, 1, 100, 1);

    hud_mr.AddCheckBox(IDC_TRACK_ANIMATE, L"Animate", x, y += db, w, h);

    hud_mr.SetVisible(false);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        dune::logger::init(L"../../data/log.txt");

        files_scene = dune::files_from_args(lpCmdLine);

        if (files_scene.size() < 2)
        {
            // the reconstructed real scene
            files_scene.push_back(L"../../data/tracked/plane.obj");

            // the last element in the vector is used as the synthetic object
            files_scene.push_back(L"../../data/tracked/happy.obj");
        }

        DXUTSetCallbackKeyboard(on_keyboard);
        DXUTSetCallbackD3D11DeviceCreated(on_create_device);
        DXUTSetCallbackD3D11FrameRender(on_render);
        DXUTSetCallbackD3D11DeviceDestroyed(dc::on_destroy_device);
        DXUTSetCallbackD3D11SwapChainResized(dc::on_resize);
        DXUTSetCallbackMsgProc(dc::on_msg);
        DXUTSetCallbackFrameMove(dc::on_frame_move);
        DXUTSetCallbackD3D11SwapChainReleasing(dc::on_releasing_swap_chain);
        DXUTSetCallbackDeviceChanging(dc::on_modify_device);

        init_gui();

        DXUTInit(true, true);
        DXUTCreateWindow(L"dirtchamber");
        DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1024, 768);

        dc::gui::update(&renderer);

        DXUTMainLoop();
    }
    catch (dune::exception& e)
    {
        tcerr << e.msg() << std::endl;
        dc::on_destroy_device(nullptr);
    }

    return DXUTGetExitCode();
}
