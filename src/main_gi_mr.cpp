/*
 * The Dirtchamber - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#include "drf_renderer.h"

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
