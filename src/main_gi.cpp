/*
 * The Dirtchamber - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#include "gi_renderer.h"

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
