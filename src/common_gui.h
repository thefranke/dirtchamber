/*
 * The Dirtchamber - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef COMMON_GUI_H
#define COMMON_GUI_H

#undef NOMINMAX
#include <DXUT.h>
#include <DXUTgui.h>

#include "idc.h"

#include "pppipe.h"

#include <dune/light_propagation_volume.h>
#include <dune/light.h>
#include <dune/sparse_voxel_octree.h>
#include <dune/composite_mesh.h>

#include <dune/deferred_renderer.h>
#include <dune/serializer.h>

typedef void(*save_func)(dune::serializer& s);
typedef void(*load_func)(dune::serializer& t);

namespace dc
{
    namespace gui
    {
        extern CDXUTDialog                             hud;
        extern CDXUTDialog                             hud_light;
        extern CDXUTDialog                             hud_gi;
        extern CDXUTDialog                             hud_postp1;
        extern CDXUTDialog                             hud_postp2;
        extern CDXUTDialogResourceManager              dlg_manager;
        extern CDXUTComboBox*                          combo_render_targets;
        extern CDXUTComboBox*                          combo_settings;

        /*! \brief Initialize the DXUT GUI with all variables in this namespace. */
        int init(PCALLBACKDXUTGUIEVENT f, int spacing = 18, UINT ewidth = 160, UINT eheight = 22);

        /*! \brief Update list of render targets in the GUI dropdown menu from a deferred renderer. */
        void update(dune::deferred_renderer* renderer);

        /*! \brief Automatically open the selected HUD matching an entry in the dropdown menu. */
        void select_hud();

        //!@{
        /*! \brief Some default callbacks. */
        LRESULT CALLBACK on_msg(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool *pbNoFurtherProcessing, void *pUserContext);
        void CALLBACK on_resize(ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
        void CALLBACK on_event(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext);
        void CALLBACK on_render(ID3D11Device* device, ID3D11DeviceContext* context, double fTime, float fElapsedTime, void* pUserContext);
        //!@}

        /*! \brief Toggle GUI off/on. */
        void toggle();

        //!@{
        /*! \brief Get the value of a GUI element. */
        bool checkbox_value(int idc);
        float slider_value(int idc, int mi, int ma);
        float slider_value(int idc, bool normalize = false);
        //!@}

        //!@{
        /*! \brief Set the value of a GUI element. */
        void set_text(int idc, LPCWSTR text);
        void set_checkbox_value(int idc, bool v);
        void set_checkbox_value(int idc, BOOL v);
        void set_slider_value(int idc, int mi, int ma, float v);
        void set_slider_value(int idc, float v, bool normalized = false);
        //!@}

        //!@{
        /*!
         * \brief Save/Load dialog to dump current renderer state.
         *
         * Each renderer has some runtime settings which can be saved or loaded. Since each parameter in the
         * GUI, be it a slider or a checkbox, is synchronized to the responsible object (for instance an LPV),
         * each objects state can be saved or retrieved from disk. To do this, these function calls open
         * a window to ask for an XML file to be saved or loaded. Each dune object which can be rendered
         * has a .parameters() function, which is a cbuffer of the parameters necessary to render the object
         * properly. Since each parameters returned cbuffer object keeps a local copy of its data, these entries
         * can be read or written, i.e. saved or loaded. A dune::serializer object manages all values in a big
         * map. Both functions only open and read/write XML files. The behavior of what to write into these
         * files is supplied as a functor, where stream operators can be used to write to the serializer.
         *
         * > void save_f(dune::serializer& s)
         * > {
         * >     s << lpv;
         * >      s << dir_light;
         * >     ...
         * > }
         *
         */
        bool save_settings(save_func f = nullptr);
        bool load_settings(load_func f = nullptr);
        //!@}

        //!@{
        /*! \brief Synchronization between GUI and a pppipe object. */
        void get_parameters(pppipe& ppp);
        void set_parameters(const pppipe& ppp);
        //!@}

        //!@{
        /*! \brief Synchronization between GUI and a directional_light object. */
        void get_parameters(dune::directional_light& main_light, dune::d3d_mesh& scene, float z_near, float z_far);
        void set_parameters(const dune::directional_light& main_light, dune::d3d_mesh& scene, float z_near, float z_far);
        //!@}

        //!@{
        /*! \brief Synchronization between GUI and a light_propagation_volume object. */
        void get_parameters(dune::light_propagation_volume& lpv);
        void set_parameters(const dune::light_propagation_volume& lpv);
        //!@}

        //!@{
        /*! \brief Synchronization between GUI and a sparse_voxel_octree object. */
        void get_parameters(dune::sparse_voxel_octree& svo);
        void set_parameters(const dune::sparse_voxel_octree& svo);
        //!@}
    }
}

#endif
