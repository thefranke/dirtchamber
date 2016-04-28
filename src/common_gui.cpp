/*
 * The Dirtchamber - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common_gui.h"
#include "idc.h"

#include "pppipe.h"

#define CPPHLSL
#include "../shader/common.h"

#include <dune/common_tools.h>
#include <dune/d3d_tools.h>
#include <dune/serializer.h>
#include <dune/camera.h>
#include <dune/math_tools.h>

#include <iomanip>

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>

namespace dc
{
    namespace gui
    {
        // gui controls
        CDXUTDialog                             hud;
        CDXUTDialog                             hud_light;
        CDXUTDialog                             hud_gi;
        CDXUTDialog                             hud_postp1;
        CDXUTDialog                             hud_postp2;
        CDXUTDialog                             hud_debug_info;
        CDXUTDialogResourceManager              dlg_manager;
        CDXUTComboBox*                          combo_render_targets = nullptr;
        CDXUTComboBox*                          combo_settings = nullptr;

        static bool enabled = true;

        void toggle()
        {
            enabled = !enabled;
        }

        CDXUTControl* find_control(int idc)
        {
            for (size_t d = 0; d < dlg_manager.m_Dialogs.size(); ++d)
            {
                auto dialog = dlg_manager.m_Dialogs[d];

                if (CDXUTControl* control = dialog->GetControl(idc))
                    return control;
            }

            return 0;
        }

        float slider_value(int idc, int mi, int ma)
        {
            CDXUTSlider* slider = dynamic_cast<CDXUTSlider*>(find_control(idc));

            float v = static_cast<float>(slider->GetValue());

            if (mi == 0 && ma == 0)
                slider->GetRange(mi, ma);

            v /= static_cast<float>(ma - mi);

            return v;
        }

        float slider_value(int idc, bool normalize)
        {
            if (normalize)
                return slider_value(idc, 0, 0);
            else
                return slider_value(idc, 0, 1);
        }

        void set_slider_value(int idc, int mi, int ma, float v)
        {
            CDXUTSlider* slider = dynamic_cast<CDXUTSlider*>(find_control(idc));

            if (mi == 0 && ma == 0)
                slider->GetRange(mi, ma);

            int iv = static_cast<int>(v * (ma - mi));

            slider->SetValue(iv);
        }

        void set_slider_value(int idc, float v, bool normalized)
        {
            if (normalized)
                set_slider_value(idc, 0, 0, v);
            else
                set_slider_value(idc, 0, 1, v);
        }

        void set_text(int idc, LPCWSTR text)
        {
            dynamic_cast<CDXUTStatic*>(find_control(idc))->SetText(text);
        }

        bool checkbox_value(int idc)
        {
            return dynamic_cast<CDXUTCheckBox*>(find_control(idc))->GetChecked();
        }

        void set_checkbox_value(int idc, bool v)
        {
            return dynamic_cast<CDXUTCheckBox*>(find_control(idc))->SetChecked(v);
        }

        void set_checkbox_value(int idc, BOOL v)
        {
            return dynamic_cast<CDXUTCheckBox*>(find_control(idc))->SetChecked(v == 1);
        }

        void CALLBACK on_render(ID3D11Device* device, ID3D11DeviceContext* context, double fTime, float fElapsedTime, void* pUserContext)
        {
            if (!enabled)
                return;

            // get FPS
            dune::tstringstream ss;
            ss << DXUTGetFPS() << L"(" << std::setprecision(2) << DXUTGetElapsedTime() * 1000 << L"ms)" << std::endl;
            hud.GetStatic(IDC_FPS_COUNTER)->SetText(ss.str().c_str());

            for (size_t i = 0; i < dlg_manager.m_Dialogs.size(); ++i)
            {
                auto h = dlg_manager.m_Dialogs[i];

                // make sure HUD is rendered last
                if (h != &hud)
                    h->OnRender(fElapsedTime);
            }

            hud.OnRender(fElapsedTime);
        }

        void select_hud()
        {
            for (UINT i = 0; i < combo_settings->GetNumItems(); ++i)
            {
                auto item = reinterpret_cast<CDXUTDialog*>(combo_settings->GetItem(i)->pData);
                if (item) item->SetVisible(i == combo_settings->GetSelectedIndex());
            }
        }

        LRESULT CALLBACK on_msg(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool *pbNoFurtherProcessing, void *pUserContext)
        {
            *pbNoFurtherProcessing = dlg_manager.MsgProc(hWnd, uMsg, wParam, lParam);
            if (*pbNoFurtherProcessing)
                return 0;

            for (size_t i = 0; i < dlg_manager.m_Dialogs.size(); ++i)
            {
                *pbNoFurtherProcessing = dlg_manager.m_Dialogs[i]->MsgProc(hWnd, uMsg, wParam, lParam);
                if (*pbNoFurtherProcessing)
                    return 0;
            }

            return 0;
        }

        void CALLBACK on_resize(ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
        {
            dune::assert_hr(dlg_manager.OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc));

            for (size_t i = 0; i < dlg_manager.m_Dialogs.size(); ++i)
                dlg_manager.m_Dialogs[i]->SetLocation(pBackBufferSurfaceDesc->Width - 165, 0);
        }

        int init(PCALLBACKDXUTGUIEVENT on_gui_event, int spacing, UINT ew, UINT eh)
        {
            const int dd = spacing;
            const int db = dd * 2;

            // add to dialog manager
            hud.Init(&dlg_manager);
            hud_light.Init(&dlg_manager);
            hud_gi.Init(&dlg_manager);
            hud_postp1.Init(&dlg_manager);
            hud_postp2.Init(&dlg_manager);
            hud_debug_info.Init(&dlg_manager);

            // set callbacks
            hud.SetCallback(on_gui_event);
            hud_light.SetCallback(on_gui_event);
            hud_gi.SetCallback(on_gui_event);
            hud_postp1.SetCallback(on_gui_event);
            hud_postp2.SetCallback(on_gui_event);
            hud_debug_info.SetCallback(on_gui_event);

            int x = 0, y = 10, w = ew, h = eh;

            hud.AddStatic(IDC_FPS_COUNTER, L"FPS: ", x, y, w, h);

            hud.AddStatic(-1, L"Render targets:", x, y += db, w, h);
            hud.AddComboBox(IDC_TARGETS, x, y += dd, w, h + 2, 0, false, &combo_render_targets);

            hud.AddStatic(-1, L"Settings for:", x, y += db, w, h);
            hud.AddComboBox(IDC_SETTINGS, x, y += dd, w, h + 2, 0, false, &combo_settings);

            int start = y;
            combo_settings->AddItem(L"", 0);

            // Lights settings
            y = start;
            combo_settings->AddItem(L"Lights", reinterpret_cast<void*>(&hud_light));

            hud_light.AddStatic(-1, L"Light direction:", x, y += db, w, h);
            hud_light.AddSlider(IDC_LIGHT_DIRECTION, x, y += dd, w, h);

            hud_light.AddStatic(-1, L"Light position X/Y/Z:", x, y += db, w, h);
            hud_light.AddSlider(IDC_LIGHT_POS_X, x, y += dd, w, h);
            hud_light.AddSlider(IDC_LIGHT_POS_Y, x, y += dd, w, h);
            hud_light.AddSlider(IDC_LIGHT_POS_Z, x, y += dd, w, h);

            hud_light.AddStatic(-1, L"Flux factor:", x, y += db, w, h);
            hud_light.AddSlider(IDC_FLUX_SCALE, x, y += dd, w, h, 0, 1000, 100);

            hud_light.AddStatic(-1, L"Warmth:", x, y += db, w, h);
            hud_light.AddSlider(IDC_LIGHT_WARMTH, x, y += dd, w, h, 0, 100, 0);

            // VPL settings
            y = start;
            combo_settings->AddItem(L"GI", reinterpret_cast<void*>(&hud_gi));

            hud_gi.AddStatic(-1, L"GI scale:", x, y += db, w, h);
            hud_gi.AddSlider(IDC_GI_SCALE, x, y += dd, w, h, 0, 100, 0);

#if defined(LPV) || defined(DLPV)
            hud_gi.AddStatic(IDC_GI_INFO1, L"LPV propagations: 0", x, y += db, w, h);
            hud_gi.AddSlider(IDC_GI_PARAMETER1, x, y += dd, w, h, 0, LPV_SIZE * 2, 0);
            hud_gi.AddStatic(-1, L"LPV Flux amplifier:", x, y += db, w, h);
#else
            hud_gi.AddStatic(-1, L"Surface roughness:", x, y += db, w, h);
#endif

            hud_gi.AddSlider(IDC_GI_PARAMETER2, x, y += db, w, h, 0, 1000, 500);

            hud_gi.AddCheckBox(IDC_GI_DEBUG1, L"Debug GI", x, y += db, w, h, false);

#if defined(LPV) || defined(DLPV)
            hud_gi.AddCheckBox(IDC_GI_DEBUG2, L"Show LPV", x, y += db, w, h, false);
#endif

            // Postprocessing 1 settings
            y = start;
            combo_settings->AddItem(L"Postprocess 1", reinterpret_cast<void*>(&hud_postp1));

            hud_postp1.AddCheckBox(IDC_FXAA_ENABLED, L"FXAA", x, y += db, w, h, true);

            hud_postp1.AddCheckBox(IDC_SSAO_ENABLED, L"SSAO", x, y += db, w, h, true);
            hud_postp1.AddStatic(-1, L"SSAO factor:", x, y += db, w, h);
            hud_postp1.AddSlider(IDC_SSAO_SCALE, x, y += dd, w, h, 0, 100, 10);

            hud_postp1.AddCheckBox(IDC_GODRAYS_ENABLED, L"Godrays", x, y += db, w, h, false);
            hud_postp1.AddStatic(-1, L"Godrays tau:", x, y += db, w, h);
            hud_postp1.AddSlider(IDC_GODRAYS_TAU, x, y += dd, w, h, 1, 10, 5);

            hud_postp1.AddCheckBox(IDC_EXPOSURE_ADAPT, L"Adapt exposure", x, y += db, w, h, false);
            hud_postp1.AddStatic(-1, L"Exposure key value:", x, y += db, w, h, true);
            hud_postp1.AddSlider(IDC_EXPOSURE_KEY, x, y += dd, w, h, 0, 100, 50);
            hud_postp1.AddStatic(-1, L"Adaptation speed:", x, y += db, w, h, true);
            hud_postp1.AddSlider(IDC_EXPOSURE_SPEED, x, y += dd, w, h, 1, 100, 20);

            // Postprocessing 2 settings
            y = start;
            combo_settings->AddItem(L"Postprocess 2", reinterpret_cast<void*>(&hud_postp2));

            hud_postp2.AddCheckBox(IDC_CRT_ENABLED, L"CRT monitor", x, y += db, w, h, false);
            hud_postp2.AddCheckBox(IDC_FILM_GRAIN_ENABLED, L"Film grain", x, y += db, w, h, false);

            hud_postp2.AddCheckBox(IDC_DOF_ENABLED, L"DOF", x, y += db, w, h, false);
            hud_postp2.AddStatic(-1, L"DOF focal plane:", x, y += db, w, h);
            hud_postp2.AddSlider(IDC_DOF_FOCAL_PLANE, x, y += dd, w, h, 0, 5000, 2000);
            hud_postp2.AddStatic(-1, L"DOF COC scale:", x, y += db, w, h);
            hud_postp2.AddSlider(IDC_DOF_COC_SCALE, x, y += dd, w, h, 50, 150, 80);

            hud_postp2.AddCheckBox(IDC_BLOOM_ENABLED, L"Bloom", x, y += db, w, h, false);
            hud_postp2.AddStatic(-1, L"Bloom sigma:", x, y += db, w, h, true);
            hud_postp2.AddSlider(IDC_BLOOM_SIGMA, x, y += dd, w, h, 50, 150, 80);
            hud_postp2.AddStatic(-1, L"Bloom treshold:", x, y += db, w, h, true);
            hud_postp2.AddSlider(IDC_BLOOM_TRESHOLD, x, y += dd, w, h, 50, 250, 150);

            // Debug panel
            y = start;
            combo_settings->AddItem(L"Debug Info", reinterpret_cast<void*>(&hud_debug_info));

            hud_debug_info.AddStatic(IDC_DEBUG_INFO + 0, L"", x, y += db, w, h);

            // startup config
            hud.SetVisible(true);
            hud_light.SetVisible(false);
            hud_postp1.SetVisible(false);
            hud_postp2.SetVisible(false);
            hud_gi.SetVisible(false);
            hud_debug_info.SetVisible(false);

            return start;
        }

        void update(dune::deferred_renderer* renderer)
        {
            if (combo_render_targets)
            {
                combo_render_targets->AddItem(L"None", IntToPtr(0));

                for (auto g = renderer->buffers().begin(); g != renderer->buffers().end(); ++g)
                {
                    dune::tstring w = g->second->name;
                    combo_render_targets->AddItem(w.c_str(), IntToPtr(1));
                }
            }
        }

        bool save_settings(save_func save_more)
        {
            TCHAR file[MAX_PATH] = L"settings.xml";

            OPENFILENAME ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"XML\0*.xml\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST;

            if (GetSaveFileName(&ofn))
            {
                dune::tstring filename = ofn.lpstrFile;

                tclog << L"Saving settings to: " << filename << std::endl;

                dune::serializer s;

                if (save_more)
                    save_more(s);

                s.save(filename);

                return true;
            }

            return false;
        }

        bool load_settings(load_func load_more)
        {
            TCHAR file[MAX_PATH] = L"settings.xml";

            OPENFILENAME ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"XML\0*.xml\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn))
            {
                dune::tstring filename = ofn.lpstrFile;

                tclog << L"Loading settings from: " << filename << std::endl;

                dune::serializer s;
                s.load(filename);

                if (load_more)
                    load_more(s);

                return true;
            }

            return false;
        }

        void get_parameters(pppipe& ppp)
        {
            auto cb = &ppp.parameters().data();
            {
                cb->bloom_enabled = checkbox_value(IDC_BLOOM_ENABLED);
                cb->bloom_sigma = slider_value(IDC_BLOOM_SIGMA, 0, 100);
                cb->bloom_treshold = slider_value(IDC_BLOOM_TRESHOLD, 0, 100);

                cb->dof_enabled = checkbox_value(IDC_DOF_ENABLED);
                cb->dof_coc_scale = slider_value(IDC_DOF_COC_SCALE, 0, 100);
                cb->dof_focal_plane = slider_value(IDC_DOF_FOCAL_PLANE);

                cb->exposure_adapt = checkbox_value(IDC_EXPOSURE_ADAPT);
                cb->exposure_key = slider_value(IDC_EXPOSURE_KEY, 0, 1000);
                cb->exposure_speed = slider_value(IDC_EXPOSURE_SPEED, 0, 50);

                cb->godrays_enabled = checkbox_value(IDC_GODRAYS_ENABLED);
                cb->godrays_tau = slider_value(IDC_GODRAYS_TAU, 0, 10000);

                cb->ssao_enabled = checkbox_value(IDC_SSAO_ENABLED);
                cb->ssao_scale = slider_value(IDC_SSAO_SCALE, true);

                cb->fxaa_enabled = checkbox_value(IDC_FXAA_ENABLED);

                cb->crt_enabled = checkbox_value(IDC_CRT_ENABLED);
                cb->film_grain_enabled = checkbox_value(IDC_FILM_GRAIN_ENABLED);
            }
        }

        void set_parameters(const pppipe& ppp)
        {
            auto cb = &ppp.parameters().data();
            {
                set_checkbox_value(IDC_BLOOM_ENABLED, cb->bloom_enabled);

                set_slider_value(IDC_BLOOM_SIGMA, 0, 100, cb->bloom_sigma);
                set_slider_value(IDC_BLOOM_TRESHOLD, 0, 100, cb->bloom_treshold);

                set_checkbox_value(IDC_DOF_ENABLED, cb->dof_enabled);
                set_slider_value(IDC_DOF_COC_SCALE, 0, 100, cb->dof_coc_scale);
                set_slider_value(IDC_DOF_FOCAL_PLANE, cb->dof_focal_plane);

                set_checkbox_value(IDC_EXPOSURE_ADAPT, cb->exposure_adapt);
                set_slider_value(IDC_EXPOSURE_KEY, 0, 1000, cb->exposure_key);
                set_slider_value(IDC_EXPOSURE_SPEED, 0, 50, cb->exposure_speed);

                set_checkbox_value(IDC_GODRAYS_ENABLED, cb->godrays_enabled);
                set_slider_value(IDC_GODRAYS_TAU, 0, 10000, cb->godrays_tau);

                set_checkbox_value(IDC_SSAO_ENABLED, cb->ssao_enabled);
                set_slider_value(IDC_SSAO_SCALE, cb->ssao_scale, true);

                set_checkbox_value(IDC_FXAA_ENABLED, cb->fxaa_enabled);

                set_checkbox_value(IDC_CRT_ENABLED, cb->crt_enabled);
                set_checkbox_value(IDC_FILM_GRAIN_ENABLED, cb->film_grain_enabled);
            }
        }

        void get_parameters(dune::light_propagation_volume& lpv)
        {
            // update constant buffer variables
            auto cb = &lpv.parameters().data();
            {
                cb->lpv_flux_amplifier = slider_value(IDC_GI_PARAMETER2, 0, 250);
                cb->vpl_scale = slider_value(IDC_GI_SCALE);
                cb->num_vpls = 0;
                cb->debug_gi = checkbox_value(IDC_GI_DEBUG1);
            }

            // update LPV propagations
            size_t num_lpv_propagations = static_cast<size_t>(slider_value(IDC_GI_PARAMETER1));
            dune::tstringstream ss;
            ss << L"LPV propagations: " << num_lpv_propagations;
            set_text(IDC_GI_INFO1, ss.str().c_str());
            lpv.set_num_propagations(num_lpv_propagations);
        }

        void set_parameters(const dune::light_propagation_volume& lpv)
        {
            // set from constant buffer variables
            auto cb = &lpv.parameters().data();
            {
                set_slider_value(IDC_GI_PARAMETER2, 0, 250, cb->lpv_flux_amplifier);
                set_slider_value(IDC_GI_SCALE, cb->vpl_scale);
                set_checkbox_value(IDC_GI_DEBUG1, cb->debug_gi);
            }

            // update LPV propagations
            set_slider_value(IDC_GI_PARAMETER1, static_cast<float>(lpv.num_propagations()));

            dune::tstringstream ss;
            ss << L"LPV propagations: " << lpv.num_propagations();
            set_text(IDC_GI_INFO1, ss.str().c_str());
        }

        void get_parameters(dune::sparse_voxel_octree& svo)
        {
            // update constant buffer variables
            auto cb = &svo.parameters().data();
            {
                cb->glossiness = slider_value(IDC_GI_PARAMETER2, 0, 250);
                cb->gi_scale = slider_value(IDC_GI_SCALE);
                cb->num_vpls = 0;
                cb->debug_gi = checkbox_value(IDC_GI_DEBUG1);
            }
        }

        void set_parameters(const dune::sparse_voxel_octree& svo)
        {
            // set from constant buffer variables
            auto cb = &svo.parameters().data();
            {
                set_slider_value(IDC_GI_PARAMETER2, 0, 250, cb->glossiness);
                set_slider_value(IDC_GI_SCALE, cb->gi_scale);
                set_checkbox_value(IDC_GI_DEBUG1, cb->debug_gi);
            }
        }

        void get_parameters(dune::directional_light& main_light, dune::d3d_mesh& scene, float z_near, float z_far)
        {
            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&scene.world());

            DirectX::XMVECTOR v_bb_min = DirectX::XMLoadFloat3(&scene.bb_min());
            DirectX::XMVECTOR v_bb_max = DirectX::XMLoadFloat3(&scene.bb_max());

            DirectX::XMVECTOR v_diag = DirectX::XMVectorSubtract(v_bb_max, v_bb_min);

            // move light(s) into world space
            DirectX::XMVECTOR d = v_diag;
            DirectX::XMVECTOR length = DirectX::XMVector3Length(d);

            d = DirectX::XMVectorSetY(d, 0.001f);
            d = DirectX::XMVectorScale(d, 0.5f);

            DirectX::XMVECTOR position = DirectX::XMVectorSubtract(v_bb_max, d);

            // direction
            float v = slider_value(IDC_LIGHT_DIRECTION) / 100.f * 2 - 1.f;

            float l = -1.f + std::abs(v);
            DirectX::XMVECTORF32 n = { v, l, 0.f, 0.f };
            DirectX::XMVECTOR normal = DirectX::XMVector4Normalize(n);

            DirectX::XMStoreFloat4(&main_light.parameters().data().direction,
                DirectX::XMVector4Normalize(DirectX::XMVector4Transform(normal, world)));

            // position
            float x = slider_value(IDC_LIGHT_POS_X) / 100.f * 2.f - 1.f;
            float y = slider_value(IDC_LIGHT_POS_Y) / 100.f * 2.f - 1.f;
            float z = slider_value(IDC_LIGHT_POS_Z) / 100.f * 2.f - 1.f;

            DirectX::XMFLOAT4 diag;
            DirectX::XMStoreFloat4(&diag, v_diag);

            DirectX::XMVECTORF32 pp = { x*diag.x, y*diag.y, z*diag.z, 0.f };

            position = DirectX::XMVectorAdd(position, pp);
            position = DirectX::XMVectorSetW(position, 1.0f);

            DirectX::XMStoreFloat4(&main_light.parameters().data().position,
                DirectX::XMVector4Transform(position, world));

            // update color
            float w = slider_value(IDC_LIGHT_WARMTH) / 100.f;

            DirectX::XMVECTORF32 flux = { 1.0f, 1.0f, 1.0f - (w / 2.f), 1.0f };

            DirectX::XMStoreFloat4(&main_light.parameters().data().flux, flux * slider_value(IDC_FLUX_SCALE, 0, 100));
        }

        void set_parameters(const dune::directional_light& main_light, dune::d3d_mesh& scene, float z_near, float z_far)
        {
            // get light direction
            set_slider_value(IDC_LIGHT_DIRECTION, (main_light.parameters().data().direction.x + 1.f) / 2.f * 100.f);

            // get flux scale
            float strength = main_light.parameters().data().flux.x;
            set_slider_value(IDC_FLUX_SCALE, 0, 100, strength);

            // get warmth
            float blue = main_light.parameters().data().flux.z / strength;
            set_slider_value(IDC_LIGHT_WARMTH, 200 - 200 * blue);

            // reconstruct position
            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&scene.world());
            DirectX::XMVECTOR v_bb_min = DirectX::XMLoadFloat3(&scene.bb_min());
            DirectX::XMVECTOR v_bb_max = DirectX::XMLoadFloat3(&scene.bb_max());
            DirectX::XMVECTOR v_diag = DirectX::XMVectorSubtract(v_bb_max, v_bb_min);
            DirectX::XMVECTOR d = v_diag;

            d = DirectX::XMVectorSetY(d, 0.001f);
            d = DirectX::XMVectorScale(d, 1 / 2.0f);

            DirectX::XMVECTOR position = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&scene.bb_max()), d);
            position = DirectX::XMVectorSetW(position, 1.f);

            DirectX::XMVECTOR v_pp = DirectX::XMLoadFloat4(&main_light.parameters().data().position);
            v_pp = DirectX::XMVectorSubtract(v_pp, position);

            DirectX::XMFLOAT4 pp;
            DirectX::XMStoreFloat4(&pp, v_pp);

            DirectX::XMFLOAT4 diag;
            DirectX::XMStoreFloat4(&diag, v_diag);

            set_slider_value(IDC_LIGHT_POS_X, (pp.x / diag.x + 1.f) / 2.f * 100.f);
            set_slider_value(IDC_LIGHT_POS_Y, (pp.y / diag.y + 1.f) / 2.f * 100.f);
            set_slider_value(IDC_LIGHT_POS_Z, (pp.z / diag.z + 1.f) / 2.f * 100.f);
        }
    }
}
