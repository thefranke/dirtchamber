/*
 * The Dirtchamber - Tobias Alexander Franke 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef COMMON_RENDERER_H
#define COMMON_RENDERER_H

#undef NOMINMAX
#include <DXUT.h>
#include <dune/dune.h>

#include "../shader/common.h"

#include "pppipe.h"
#include "skydome.h"

extern std::vector<dune::tstring>       files_scene;
extern float                            z_near;
extern float                            z_far;

/*! \brief Dirtchamber shared code. */
namespace dc
{
    /*!
     * \brief A common, simple deferred renderer.
     *
     * Implements a basic deferred renderer with a camera, a skydome, a scene composed
     * from one or more models (gilga_mesh) and a postprocessor with SSAO, Bloom, HDR Rendering,
     * TV grain, Depth of Field, FXAA, Godrays etc.
     *
     * The common_renderer renders the camera view into a GBuffer with the following layout:
     * - **color** : DXGI_FORMAT_R16G16B16A16_FLOAT
     * - **specular** : DXGI_FORMAT_R16G16B16A16_FLOAT
     * - **normals** : DXGI_FORMAT_R10G10B10A2_UNORM
     * - **linear depth and variance** : DXGI_FORMAT_R32G32_FLOAT
     *
     * The postprocessor target is a DXGI_FORMAT_R16G16B16A16_FLOAT render_target.
     */
    class common_renderer : public dune::deferred_renderer
    {
    protected:
        struct per_frame
        {
            float time;
            float time_delta;
            DirectX::XMFLOAT2 pad0;
        };

        struct onetime
        {
            DirectX::XMFLOAT4 scene_dim_max;
            DirectX::XMFLOAT4 scene_dim_min;
        };

    protected:
        dune::camera camera_;
        dune::cbuffer<per_frame> cb_per_frame_;
        dune::cbuffer<onetime> cb_onetime_;
        dune::gilga_mesh debug_box_;
        dune::composite_mesh scene_;
        dune::gbuffer def_;
        skydome sky_;

        DirectX::XMFLOAT3 bb_min_;
        DirectX::XMFLOAT3 bb_max_;

        pppipe postprocessor_;
        ID3D11ShaderResourceView* noise_srv_;

    protected:
        /*! \brief Derived classes can overload this method to call render() on more dune objects. Buffers etc. are all set up already. */
        virtual void do_render_scene(ID3D11DeviceContext* context);

        /*! \brief Resets all render target views to nullptr. */
        void reset_omrtv(ID3D11DeviceContext* context);

    protected:
        /*! \brief Upload the bounding box of a mesh to the pixel shader */
        void update_bbox(ID3D11DeviceContext* context, dune::d3d_mesh& mesh);

        /*! \brief Upload camera parameters to the respective shaders. */
        void update_camera_parameters(ID3D11DeviceContext* context);

        /*! \brief Update the scene with a new model matrix to move/scale/... stuff. */
        void update_scene(ID3D11DeviceContext* context, const DirectX::XMMATRIX& world /* = DirectX::XMMatrixIdentity() */);

        /*! \brief Upload one-time parameters (really just once) such as noise textures. */
        void update_onetime_parameters(ID3D11DeviceContext* context, dune::d3d_mesh& mesh);

        /*! \brief Clear the GBufer and render the scene. */
        void render_scene(ID3D11DeviceContext* context, float* clear_color, ID3D11DepthStencilView* dsv);

    public:
        virtual void create(ID3D11Device* device);
        virtual void destroy();
        virtual void resize(UINT width, UINT height);
        virtual void render(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer, ID3D11DepthStencilView* dsv);

        /*! \brief Upload postprocessing parameters from the postprocessing pipeline. */
        virtual void update_postprocessing_parameters(ID3D11DeviceContext* context);

        /*! \brief Update function for everything called once a frame. */
        virtual void update_frame(ID3D11DeviceContext* context, double time, float elapsed_time);

        /*! \brief Update function called for the camera. */
        virtual void update_camera(ID3D11DeviceContext* context, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        /*! \brief A function called if the shaders should be reloaded. */
        virtual void reload_shader(ID3D11Device* device, ID3D11DeviceContext* context) {}

        /*!
         * \brief Set deferred rendering shaders.
         *
         * Sets the complete shaders for a deferred renderer, which includes an input_binary for a fullscreen triangle,
         * a vertex and pixel shader for deferred rendering, an overlay shader which displays GBuffer textures for
         * debugging, a start slot for the deferred GBuffer, a slot for the current GBuffer texture to render on an overlay
         * and a postprocessing start slot where the postprocessor expects the frontbuffer (i.e. the result of the deferred pass)
         * is located at.
         *
         * \param device The Direct3D device.
         * \param input_binary The input layout for the fullscreen triangle.
         * \param vs_deferred The vertex shader of the deferred renderer
         * \param ps_deferred The pixel shader of the deferred renderer
         * \param ps_overlay A shader which displays an overlay of a selected GBuffer texture for debugging purposes
         * \param start_slot The slot at which the GBuffer starts
         * \param overlay_slot A slot at which the currently selected GBuffer texture is located. Call overlay() to render the texture.
         * \param postprocessing_slot The slot at which the final deferred buffer is located at.
         */
        void set_shader(ID3D11Device* device,
            ID3DBlob* input_binary,
            ID3D11VertexShader* vs_deferred,
            ID3D11PixelShader* ps_deferred,
            ID3D11PixelShader* ps_overlay,
            UINT start_slot,
            UINT overlay_slot,
            UINT postprocessing_slot);

        inline pppipe& postprocessor() { return postprocessor_; }
        inline dune::camera& camera() { return camera_; }
        inline dune::composite_mesh& scene() { return scene_; }

        /*! \brief Saves dune objects used in the common_renderer. Overload to add more. */
        virtual void save(dune::serializer& s);

        /*! \brief Load dune objects used in the common_renderer. Overload to add more. */
        virtual void load(ID3D11DeviceContext* context, const dune::serializer& s);
    };

    /*!
     * \brief A common_renderer also rendering an RSM.
     *
     * The rsm_renderer derives from a common_renderer and adds one light source of type L.
     * For this light source, an RSM is generated. Depending on the type of the light source, there
     * may be two or more RSMs.
     */
    template<typename L = dune::directional_light>
    class rsm_renderer : public dc::common_renderer
    {
    protected:
        L main_light_;
        dune::render_target rsm_depth_;
        dune::render_target dummy_spec_;
        bool update_rsm_;

    protected:
        /*! \brief Update and upload RSM camera parameters (i.e. light view projection matrix etc.). */
        void update_rsm_camera_parameters(ID3D11DeviceContext* context, dune::directional_light& l)
        {
            auto cb = &camera_.parameters().data();
            {
                XMStoreFloat4x4(&cb->vp,
                    DirectX::XMLoadFloat4x4(&l.parameters().data().vp));

                XMStoreFloat4x4(&cb->vp_inv,
                    XMLoadFloat4x4(&l.parameters().data().vp_inv));

                cb->camera_pos = DirectX::XMFLOAT3
                (
                    l.parameters().data().position.x,
                    l.parameters().data().position.y,
                    l.parameters().data().position.z
                );

                cb->z_far = z_far;
            }
            camera_.parameters().to_vs(context, SLOT_CAMERA_VS);
        }

        /*! \brief Render a scene from a directional_light position into an RSM. */
        void render_rsm(ID3D11DeviceContext* context, float* clear_color, dune::gbuffer& rsm)
        {
            // render rsm
            ID3D11RenderTargetView* rsm_views[] = {
                rsm[L"colors"]->rtv(),
                rsm[L"normals"]->rtv(),
                rsm[L"lineardepth"]->rtv(),
                dummy_spec_.rtv()
            };

            ID3D11DepthStencilView* dsv = rsm_depth_.dsv();

            context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0, 0);
            dune::clear_rtvs(context, rsm_views, 2, clear_color);

            float cc[4] = { z_far, z_far, z_far, z_far };
            dune::clear_rtvs(context, rsm_views + 2, 1, cc);

            DirectX::XMFLOAT2 size = rsm[L"colors"]->size();
            dune::set_viewport(context,
                static_cast<size_t>(size.x),
                static_cast<size_t>(size.y));

            context->OMSetRenderTargets(4, rsm_views, dsv);

            do_render_rsm(context);

            reset_omrtv(context);
        }

        /*! \brief Virtual function to render more stuff for the RSM. */
        virtual void do_render_rsm(ID3D11DeviceContext* context)
        {
            scene_.set_shader_slots(SLOT_TEX_DIFFUSE, SLOT_TEX_NORMAL);

            for (size_t x = 0; x < scene_.size(); ++x)
            {
                dune::gilga_mesh* m = dynamic_cast<dune::gilga_mesh*>(scene_[x].get());
                if (m) m->set_alpha_slot(SLOT_TEX_ALPHA);
            }

            scene_.render(context);
        }

    public:
        virtual void create(ID3D11Device* device)
        {
            common_renderer::create(device);

            auto ld_color  = def_[L"colors"]->desc();
            auto ld_normal = def_[L"normals"]->desc();
            auto ld_ldepth = def_[L"lineardepth"]->desc();
            auto ld_spec   = def_[L"specular"]->desc();

            // setup reflective shadow map
            ld_color.Width = ld_normal.Width = ld_ldepth.Width = ld_spec.Width =
            ld_color.Height = ld_normal.Height = ld_ldepth.Height = ld_spec.Height = 1024;

            dummy_spec_.create(device, ld_spec);

            main_light_.create(device, L"rsm", ld_color, ld_normal, ld_ldepth);

            DXGI_SURFACE_DESC ld_rsmdepth;
            ld_rsmdepth.Format = DXGI_FORMAT_R32_TYPELESS;
            ld_rsmdepth.Width = 1024;
            ld_rsmdepth.Height = 1024;
            ld_rsmdepth.SampleDesc.Count = 1;
            ld_rsmdepth.SampleDesc.Quality = 0;
            rsm_depth_.create(device, ld_rsmdepth);

            // add buffers to deferred renderer
            add_buffer(*main_light_.rsm()[L"lineardepth"], SLOT_TEX_SVO_RSM_RHO_START);
        }

        /*! \brief Update and upload parameters (view projection matrix, flux etc.) of a directional light. */
        void update_light_parameters(ID3D11DeviceContext* context, dune::directional_light& l)
        {
            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&scene_.world());

            // TODO: make better
            DirectX::XMVECTOR up = { -1.f, 0.f, 0.f, 0.f };
            DirectX::XMVECTOR upt;
            upt = DirectX::XMVector4Transform(up, world);
            upt = DirectX::XMVector4Normalize(upt);

            DirectX::XMFLOAT4 upt_vec;
            DirectX::XMStoreFloat4(&upt_vec, upt);

            // projection matrix
            auto light_proj = dune::make_projection(z_near, z_far);

            l.update(upt_vec, light_proj);

            l.parameters().to_ps(context, SLOT_LIGHT_PS);

            update_rsm_ = true;
        }

        virtual void destroy()
        {
            common_renderer::destroy();
            main_light_.destroy();
            rsm_depth_.destroy();
            dummy_spec_.destroy();
        }

        inline dune::directional_light& main_light()
        {
            return main_light_;
        }

        virtual void save(dune::serializer& s)
        {
            common_renderer::save(s);
            s << main_light_;
        }

        virtual void load(ID3D11DeviceContext* context, const dune::serializer& s)
        {
            common_renderer::load(context, s);

            s >> main_light_;
        }
    };
}

#endif
