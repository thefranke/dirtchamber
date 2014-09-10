/*
 * Global illumination renderer by Tobias Alexander Franke (tob@cyberhead.de) 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#include <dune/dune.h>

#undef NOMINMAX
#include "common_renderer.h"
#include "common_dxut.h"
#include "common_gui.h"

/*!
 * \brief A full global illumination renderer.
 *
 * This class implements an rsm_renderer and enhances it to provide a full scale GI solution.
 * This is done via Light Propagation Volumes or Voxel Cone Tracing. Because both are volumetric
 * solutions and share almost all code, you can switch between both with a define \#LPV. If \#LPV
 * is set, a Light Propagation Volume is used, otherwise a Sparse Voxel Octree will be instaniated.
 */
class gi_renderer : public dc::rsm_renderer<dune::directional_light>
{
public:
    float time_deferred_;
    float time_rsm_;

protected:
    dune::profile_query profiler_;

#ifdef LPV
    dune::light_propagation_volume volume_;
    #define VOLUME_SIZE LPV_SIZE
    #define VOLUME_PARAMETERS_SLOT SLOT_LPV_PARAMETERS_VS_PS
#else
    dune::sparse_voxel_octree volume_;
    #define VOLUME_SIZE SVO_SIZE
    #define VOLUME_PARAMETERS_SLOT SLOT_SVO_PARAMETERS_VS_GS_PS
#endif

public:
    virtual void create(ID3D11Device* device)
    {
        rsm_renderer::create(device);

        // create sparse voxel octree
        volume_.create(device, VOLUME_SIZE);

        profiler_.create(device);

        load_shader(device);
    }

    virtual void destroy()
    {
        rsm_renderer::destroy();
        volume_.destroy();
        profiler_.destroy();
    }

    virtual void render(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer, ID3D11DepthStencilView* dsv)
    {
        static float clear_color[4] = { 0.0f, 0.f, 0.0f, 1.0f };

        render_gi(context, clear_color);
        render_scene(context, clear_color, dsv);

        context->GenerateMips(main_light_.rsm()[L"lineardepth"]->srv());
        context->GenerateMips(def_[L"lineardepth"]->srv());

        dune::set_viewport(context, DXUTGetWindowWidth(), DXUTGetWindowHeight());

        profiler_.begin(context);
        deferred_renderer::render(context, backbuffer, dsv);
        time_deferred_ = profiler_.result();
    }

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
        ID3D11GeometryShader* gs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        ID3D11PixelShader* ps1 = nullptr;

        auto cleanup = [&](){ dune::safe_release(vs_blob); dune::safe_release(vs); dune::safe_release(ps); dune::safe_release(ps1); dune::safe_release(gs); };

        // mesh shader
        dune::compile_shader(device, L"../../shader/d3d_mesh.hlsl", "vs_5_0", "vs_mesh", shader_flags, nullptr, &vs, &vs_blob);
        dune::compile_shader(device, L"../../shader/d3d_mesh.hlsl", "ps_5_0", "ps_mesh", shader_flags, nullptr, &ps);
        scene_.set_shader(device, vs_blob, vs, ps);
        cleanup();

        // deferred shader
        dune::compile_shader(device, L"../../shader/fs_triangle.hlsl", "vs_5_0", "vs_fs_triangle", shader_flags, nullptr, &vs, &vs_blob);
#ifdef LPV
        dune::compile_shader(device, L"../../shader/deferred_lpv.hlsl", "ps_5_0", "ps_lpv", shader_flags, nullptr, &ps);
#else
        dune::compile_shader(device, L"../../shader/deferred_vct.hlsl", "ps_5_0", "ps_vct", shader_flags, nullptr, &ps);
#endif
        dune::compile_shader(device, L"../../shader/overlay.hlsl", "ps_5_0", "ps_overlay", shader_flags, nullptr, &ps1);
        set_shader(device, vs_blob, vs, ps, ps1, SLOT_TEX_DEFERRED_START, SLOT_TEX_OVERLAY, SLOT_TEX_POSTPROCESSING_START);
        cleanup();

        // skydome shader
        dune::compile_shader(device, L"../../shader/d3d_mesh_skydome.hlsl", "vs_5_0", "vs_skydome", shader_flags, nullptr, &vs, &vs_blob);
        dune::compile_shader(device, L"../../shader/d3d_mesh_skydome.hlsl", "ps_5_0", "ps_skydome", shader_flags, nullptr, &ps);
        sky_.set_shader(device, vs_blob, vs, ps);
        cleanup();

#ifdef LPV
        // lpv inject shader
        dune::compile_shader(device, L"../../shader/lpv_inject.hlsl", "vs_5_0", "vs_lpv_inject", shader_flags, nullptr, &vs);
        dune::compile_shader(device, L"../../shader/lpv_inject.hlsl", "gs_5_0", "gs_lpv_inject", shader_flags, nullptr, &gs);
        dune::compile_shader(device, L"../../shader/lpv_inject.hlsl", "ps_5_0", "ps_lpv_inject", shader_flags, nullptr, &ps);
        dune::compile_shader(device, L"../../shader/lpv_normalize.hlsl", "ps_5_0", "ps_lpv_normalize", shader_flags, nullptr, &ps1);
        volume_.set_inject_shader(vs, gs, ps, ps1, SLOT_TEX_LPV_INJECT_RSM_START);
        cleanup();

        // lpv propagate shader
        dune::compile_shader(device, L"../../shader/lpv_propagate.hlsl", "vs_5_0", "vs_lpv_propagate", shader_flags, nullptr, &vs, &vs_blob);
        dune::compile_shader(device, L"../../shader/lpv_propagate.hlsl", "gs_5_0", "gs_lpv_propagate", shader_flags, nullptr, &gs);
        dune::compile_shader(device, L"../../shader/lpv_propagate.hlsl", "ps_5_0", "ps_lpv_propagate", shader_flags, nullptr, &ps);
        volume_.set_propagate_shader(device, vs, gs, ps, vs_blob, SLOT_TEX_LPV_PROPAGATE_START);
        cleanup();

        // lpv volume visualization
        dune::compile_shader(device, L"../../shader/lpv_rendervol.hlsl", "vs_5_0", "vs_rendervol", shader_flags, nullptr, &vs, &vs_blob);
        dune::compile_shader(device, L"../../shader/lpv_rendervol.hlsl", "ps_5_0", "ps_rendervol", shader_flags, nullptr, &ps);
        debug_box_.set_shader(device, vs_blob, vs, ps);
        cleanup();
#else
        // svo voxelize shader
        dune::compile_shader(device, L"../../shader/svo_voxelize.hlsl", "vs_5_0", "vs_svo_voxelize", shader_flags, nullptr, &vs);
        dune::compile_shader(device, L"../../shader/svo_voxelize.hlsl", "gs_5_0", "gs_svo_voxelize", shader_flags, nullptr, &gs);
        dune::compile_shader(device, L"../../shader/svo_voxelize.hlsl", "ps_5_0", "ps_svo_voxelize", shader_flags, nullptr, &ps);
        volume_.set_shader_voxelize(vs, gs, ps);
        cleanup();

        // svo inject shader
        dune::compile_shader(device, L"../../shader/svo_inject.hlsl", "vs_5_0", "vs_svo_inject", shader_flags, nullptr, &vs);
        dune::compile_shader(device, L"../../shader/svo_inject.hlsl", "ps_5_0", "ps_svo_inject", shader_flags, nullptr, &ps);
        volume_.set_shader_inject(vs, ps, SLOT_TEX_SVO_RSM_RHO_START);
        cleanup();
#endif
    }

    /*! \brief Render/compute the GI volume, whether it be LPV or SVO. */
    void render_volume(ID3D11DeviceContext* context, float* clear_color)
    {
#ifdef LPV
        ID3D11ShaderResourceView* null_srv[] = { nullptr, nullptr, nullptr };
        context->PSSetShaderResources(SLOT_TEX_LPV_DEFERRED_START, 3, null_srv);

        // TODO: inject main_light, not main_light.rsm()
        volume_.inject(context, main_light_.rsm(), true);
        volume_.render(context);
        volume_.to_ps(context, SLOT_TEX_LPV_DEFERRED_START);
#else
        // voxelize scene
        for (size_t x = 0; x < scene_.size(); ++x)
        {
            dune::gilga_mesh* m = dynamic_cast<dune::gilga_mesh*>(scene_[x].get());
            m->set_shader_slots(SLOT_TEX_DIFFUSE);
            if (m) volume_.voxelize(context, *m, x == 0);
        }

        volume_.inject(context, main_light_);

        // prefilter volume
        volume_.filter(context);

        // upload to gpu
        volume_.to_ps(context, SLOT_TEX_SVO_V_START);
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
            profiler_.begin(context);
            render_rsm(context, clear_color, main_light_.rsm());
            time_rsm_ = profiler_.result();

            // render gi volume
            render_volume(context, clear_color);

            update_rsm_ = false;
        }
    }

public:
    void update_gi_parameters(ID3D11DeviceContext* context)
    {
        volume_.set_model_matrix(the_context, scene_.world(), bb_min_, bb_max_, SLOT_LPV_PARAMETERS_VS_PS);
        volume_.parameters().to_ps(context, SLOT_GI_PARAMETERS_PS);
        update_rsm_ = true;
    }

    void update_everything(ID3D11DeviceContext* context)
    {
        update_scene(context, DirectX::XMMatrixIdentity());
        update_camera_parameters(context);
        update_light_parameters(context, main_light_);
        update_onetime_parameters(context, scene_);
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
        s << volume_;
    }

    virtual void load(ID3D11DeviceContext* context, const dune::serializer& s)
    {
        rsm_renderer::load(context, s);

        s >> volume_;

        update_everything(context);
    }

    inline auto volume() -> decltype((volume_))
    {
        return volume_;
    }
};

gi_renderer renderer;

void update_timings_gui()
{
    dune::tstringstream ss;

    ss  << L"RSM: " << renderer.time_rsm_ << L"ms\n"
#ifdef LPV
        << L"Inject: " << renderer.volume().time_inject_ << L"ms\n"
        << L"Normalize: " << renderer.volume().time_normalize_ << L"ms\n"
        << L"Propagate: " << renderer.volume().time_propagate_ << L"ms\n"
#else
        << L"Inject: " << renderer.volume().time_inject_ << L"ms\n"
        << L"Voxelize: " << renderer.volume().time_voxelize_ << L"ms\n"
        << L"Filtering: " << renderer.volume().time_mip_ << L"ms\n"
#endif
        << L"Deferred: " << renderer.time_deferred_ << "ms\n"
        ;

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
            dc::gui::set_parameters(renderer.volume());
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

    dc::gui::get_parameters(renderer.volume());
    dc::gui::get_parameters(renderer.postprocessor());
    dc::gui::get_parameters(renderer.main_light(), renderer.scene(), z_near, z_far);

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
        dc::gui::get_parameters(renderer.volume());
        renderer.update_gi_parameters(the_context);
    }

    if (current_tab == &dc::gui::hud_light)
    {
        dc::gui::get_parameters(renderer.main_light(), renderer.scene(), z_near, z_far);
        renderer.update_light_parameters(the_context, renderer.main_light());
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        dune::logger::init(L"../../data/log.txt");

        files_scene = dune::files_from_args(lpCmdLine, L"../../data/cornellbox/cornellbox.obj");

        DXUTSetCallbackKeyboard(on_keyboard);
        DXUTSetCallbackD3D11DeviceCreated(on_create_device);
        DXUTSetCallbackD3D11FrameRender(on_render);
        DXUTSetCallbackD3D11DeviceDestroyed(dc::on_destroy_device);
        DXUTSetCallbackD3D11SwapChainResized(dc::on_resize);
        DXUTSetCallbackMsgProc(dc::on_msg);
        DXUTSetCallbackFrameMove(dc::on_frame_move);
        DXUTSetCallbackD3D11SwapChainReleasing(dc::on_releasing_swap_chain);
        DXUTSetCallbackDeviceChanging(dc::on_modify_device);

        dc::gui::init(on_gui_event);

        DXUTInit(true, true);
        DXUTCreateWindow(L"dirtchamber");
        DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1280, 720);

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