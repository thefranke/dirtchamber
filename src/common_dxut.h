/*
 * The Dirtchamber - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef COMMON_DXUT
#define COMMON_DXUT

#include "common_renderer.h"

#include <DXUTgui.h>

extern dc::common_renderer*             the_renderer;
extern ID3D11Device*                    the_device;
extern ID3D11DeviceContext*             the_context;

namespace dc
{
    //!@{
    /*!
     * \brief Default implementation of DXUT callbacks common to all render samples.
     *
     * All render samples share common DXUT function callbacks which use identlical code. They all refer to a
     * common_renderer pointer the_renderer which represents the main implementation and which must be set when initializing
     * the sample
     */
    void CALLBACK on_releasing_swap_chain(void* pUserContext);
    bool CALLBACK on_modify_device(DXUTDeviceSettings* settings, void* user_context);
    HRESULT CALLBACK on_resize(ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
    void CALLBACK on_frame_move(double fTime, float fElapsedTime, void* pUserContext);
    LRESULT CALLBACK on_msg(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool *pbNoFurtherProcessing, void *pUserContext);
    void CALLBACK on_destroy_device(void* pUserContext);
    void CALLBACK on_keyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext);
    void CALLBACK on_render(ID3D11Device* device, ID3D11DeviceContext* context, double fTime, float fElapsedTime, void* pUserContext);
    void CALLBACK on_gui_event(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext);
    //!@}
}

#endif
