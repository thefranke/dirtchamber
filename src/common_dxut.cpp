/*
 * common_dxut.h by Tobias Alexander Franke (tob@cyberhead.de) 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common_dxut.h"
#include "common_gui.h"

namespace dc
{
    void CALLBACK on_releasing_swap_chain(void* pUserContext)
    {
        dc::gui::dlg_manager.OnD3D11ReleasingSwapChain();
    }

    bool CALLBACK on_modify_device(DXUTDeviceSettings* settings, void* user_context)
    {
#if defined(DEBUG) | defined(_DEBUG)
        settings->d3d11.CreateFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif

        return true;
    }

    void CALLBACK on_render(ID3D11Device* device, ID3D11DeviceContext* context, double fTime, float fElapsedTime, void* pUserContext)
    {
        static float clear_color[4] = { 0.0f, 0.f, 0.0f, 1.0f };

        if (the_renderer)
            the_renderer->render(context, DXUTGetD3D11RenderTargetView(), DXUTGetD3D11DepthStencilView());

        // render gui
        dc::gui::on_render(device, context, fTime, fElapsedTime, pUserContext);
    }

    HRESULT CALLBACK on_resize(ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
    {
        tclog << L"State: Resizing" << std::endl;

        if (the_renderer)
            the_renderer->resize(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);

        dc::gui::on_resize(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc, pUserContext);

        return S_OK;
    }

    void CALLBACK on_frame_move(double fTime, float fElapsedTime, void* pUserContext)
    {
        if (the_renderer)
            the_renderer->update_frame(the_context, fTime, fElapsedTime);
    }

    LRESULT CALLBACK on_msg(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool *pbNoFurtherProcessing, void *pUserContext)
    {
        dc::gui::on_msg(hWnd, uMsg, wParam, lParam, pbNoFurtherProcessing, pUserContext);

        if (*pbNoFurtherProcessing)
            return 0;

        if (the_renderer)
            the_renderer->update_camera(the_context, hWnd, uMsg, wParam, lParam);

        return 0;
    }

    void CALLBACK on_destroy_device(void* pUserContext)
    {
        tclog << L"State: Destroy" << std::endl;

        if (the_renderer)
            the_renderer->destroy();

        dc::gui::dlg_manager.OnD3D11DestroyDevice();
    }

    void CALLBACK on_keyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
    {
        if (!the_renderer)
            return;

        static const auto save_stuff = [](dune::serializer& s){ the_renderer->save(s); };
        static const auto load_stuff = [](dune::serializer& s){ the_renderer->load(the_context, s); };

        if (nChar == 'C' && bKeyDown)
        {
            static bool enable = false;
            enable = !enable;

            if (enable)
                the_renderer->start_capture(the_device, DXUTGetWindowWidth(), DXUTGetWindowHeight(), 30);
            else
                the_renderer->stop_capture();
        }

        if (nChar == 'R' && bKeyDown)
            the_renderer->reload_shader(the_device, the_context);

        if (nChar == 'O' && bKeyDown)
            dc::gui::toggle();

        if (nChar == 'P' && bKeyDown)
            dc::gui::save_settings(save_stuff);
    }

    void CALLBACK on_gui_event(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
    {
        if (!the_renderer)
            return;

        CDXUTDialog* current_tab = pControl->m_pDialog;

        if (current_tab == &dc::gui::hud_postp1 ||
            current_tab == &dc::gui::hud_postp2)
        {
            dc::gui::get_parameters(the_renderer->postprocessor());
            the_renderer->update_postprocessing_parameters(the_context);
        }

        switch (nControlID)
        {
        case IDC_TOGGLE_FULLSCREEN:
            DXUTToggleFullScreen();
            break;

        case IDC_TARGETS:
            the_renderer->show_target(the_context, dc::gui::combo_render_targets->GetSelectedItem()->strText);
            break;

        case IDC_SETTINGS:
            dc::gui::select_hud();
            break;
        }
    }
}
