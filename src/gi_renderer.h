/*
 * The Dirtchamber - Tobias Alexander Franke 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef GI_RENDERER_H
#define GI_RENDERER_H

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
 * is set, a Light Propagation Volume is used, otherwise a Sparse Voxel Octree will be instantiated.
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

        auto cleanup = [&]() { dune::safe_release(vs_blob); dune::safe_release(vs); dune::safe_release(ps); dune::safe_release(ps1); dune::safe_release(gs); };

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

#endif
