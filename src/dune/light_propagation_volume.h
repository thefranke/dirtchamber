/*
 * Dune D3D library - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_LIGHT_PROPAGATION_VOLUME
#define DUNE_LIGHT_PROPAGATION_VOLUME

#include <D3D11.h>

#include "render_target.h"
#include "shader_resource.h"
#include "cbuffer.h"
#include "d3d_tools.h"

namespace dune
{
    class gbuffer;
    struct d3d_mesh;
}

namespace dune
{
    /*!
     * \brief A Light Propagation Volume.
     *
     * This class manages all resources and rendering steps necessary to create a
     * Light Propagation Volume (LPV). This includes a variety of shaders and
     * several volume textures, as well as constant buffers to transform from
     * world to volume space etc.
     */
    class light_propagation_volume : public shader_resource
    {
    public:
        float time_inject_;
        float time_propagate_;
        float time_normalize_;

        struct param
        {
            float vpl_scale;
            float lpv_flux_amplifier;
            UINT num_vpls;
            BOOL debug_gi;
        };

    protected:
        ID3D11BlendState*       bs_inject_;
        ID3D11BlendState*       bs_propagate_;

        sampler_state           ss_vplfilter_;

        ID3D11VertexShader*     vs_inject_;
        ID3D11GeometryShader*   gs_inject_;
        ID3D11PixelShader*      ps_inject_;

        ID3D11VertexShader*     vs_propagate_;
        ID3D11GeometryShader*   gs_propagate_;
        ID3D11PixelShader*      ps_propagate_;

        ID3D11PixelShader*      ps_normalize_;

        INT                     propagate_start_slot_;
        INT                     inject_rsm_start_slot_;

        ID3D11Buffer*           lpv_volume_;
        ID3D11InputLayout*      input_layout_;
        UINT                    volume_size_;

        render_target           lpv_r_[2];
        render_target           lpv_g_[2];
        render_target           lpv_b_[2];

        render_target           lpv_accum_r_;
        render_target           lpv_accum_g_;
        render_target           lpv_accum_b_;

        render_target           lpv_inject_counter_;

        size_t                  iterations_rendered_;

        unsigned int            curr_;
        unsigned int            next_;

        DirectX::XMFLOAT4X4 world_to_lpv_;

        struct cbs_debug
        {
            DirectX::XMFLOAT3 lpv_pos;
            float pad;
        };

        cbuffer<cbs_debug> cb_debug_;

        struct cbs_parameters
        {
            DirectX::XMFLOAT4X4 world_to_lpv;
            UINT lpv_size;
            DirectX::XMFLOAT3 pad;
        };

        cbuffer<cbs_parameters> cb_parameters_;

        struct cbs_propagation
        {
            UINT iteration;
            DirectX::XMFLOAT3 pad;
        };

        cbuffer<cbs_propagation> cb_propagation_;

        typedef cbuffer<param> cb_gi_parameters;
        cb_gi_parameters cb_gi_parameters_;

        dune::profile_query profiler_;

    protected:
        void swap_buffers();

        void normalize(ID3D11DeviceContext* context);
        void propagate(ID3D11DeviceContext* context);
        void propagate(ID3D11DeviceContext* context, size_t num_iterations);

    public:
        light_propagation_volume();
        virtual ~light_propagation_volume() {}

        virtual void create(ID3D11Device* device, UINT volume_size);
        virtual void destroy();

        //!@{
        /*! \brief Return local cbuffer parameters. */
        cb_gi_parameters& parameters() { return cb_gi_parameters_; }
        const cb_gi_parameters& parameters() const { return cb_gi_parameters_; }
        //!@}

        /*!
         * \brief Set the complete injection shader.
         *
         * Set the injection shader for the LPV, which includes vertex, geometry and pixel shader, as well
         * as a normalization pixel shader. The injection reads VPLs from a gbuffer bound to texture slots
         * starting at inject_rsm_start_slot, and after the injection normalizes it (i.e. divides each voxel
         * by the number of injects it received).
         *
         * \param vs The injection vertex shader.
         * \param gs The injection geometry shader.
         * \param ps The injection pixel shader.
         * \param ps_normalize The normalization pixel shader.
         * \param inject_rsm_start_slot The slot at which all gbuffer textures will be bound consecutively.
         */
        void set_inject_shader(ID3D11VertexShader* vs, ID3D11GeometryShader* gs, ID3D11PixelShader* ps, ID3D11PixelShader* ps_normalize, UINT inject_rsm_start_slot);

        /*!
         * \brief Set the complete propagation shader.
         *
         * Set the propagation shader for the LPV, which includes vertex, geometry and pixel shader, a binary input layout
         * and a starting slot for the volumes which are currently read from.
         *
         * The propagation works by switching between a front and backbuffer volume for the LPV. In each step, the previously rendered volume
         * is bound as input and read from to propagate energy and write it to a new volume. The three volumes for red, green and blue spherical
         * harmonic components are bound to inject_rsm_start_slot consecutively. Since the geometry shader is in charge of scattering radiance to
         * neighbor voxels, it also needs an input layout for some basic vertices.
         *
         * \param device The Direct3d device.
         * \param vs The propagation vertex shader.
         * \param gs The propagation geometry shader.
         * \param ps The propagation pixel shader.
         * \param input_binary A blob of the shaders input layout.
         * \param propagate_start_slot The slot at which the propagation shader expects three volumes for red, green and blue spherical harmonic components.
         */
        void set_propagate_shader(ID3D11Device* device, ID3D11VertexShader* vs, ID3D11GeometryShader* gs, ID3D11PixelShader* ps, ID3DBlob* input_binary, UINT propagate_start_slot);

        /*!
         * \brief Inject VPLs from an RSM into the LPV.
         *
         * Injects VPLs generated from the parameter rsm into the LPV. The exact method to generate VPLs is implementation-dependent,
         * but the behavior can be overwritten.
         *
         * \param context A Direc3D context.
         * \param rsm A reflective shadow map with colors, normals and lineardepth.
         * \param clear If true, the volume will be cleared before injection.
         */
        virtual void inject(ID3D11DeviceContext* context, gbuffer& rsm, bool clear);

        /*! \brief Run the normalization and propagation of the LPV. */
        void render(ID3D11DeviceContext* context);

        //!@{
        /*! \brief Get/set the number of propagation steps for the LPV. */
        size_t num_propagations() const { return iterations_rendered_; }
        void set_num_propagations(size_t n) { iterations_rendered_ = n; }
        //!@}

        /*! \brief A rendering function to visualize the LPV with colored cubes for each voxel. */
        void visualize(ID3D11DeviceContext* context, d3d_mesh* node, UINT debug_info_slot);

        //!@{
        /*! \brief Set/get the world -> LPV matrix, which transforms world coordinates to LPV volume coordinates. */
        void set_model_matrix(ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& model, const DirectX::XMFLOAT3& lpv_min, const DirectX::XMFLOAT3& lpv_max, UINT lpv_parameters_slot);
        const DirectX::XMFLOAT4X4& world_to_lpv() const { return world_to_lpv_; }
        //!@}

        virtual void to_ps(ID3D11DeviceContext* context, UINT slot);
    };

    /*!
     * \brief A Delta Light Propagation Volume (DLPV).
     *
     * This class is an implementation of [[Franke 2013]](http://www.tobias-franke.eu/publications/franke13dlpv).
     * A DLPV is an LPV that extracts the difference in illumination caused by the introduction
     * of an additional object into a scene. It can be used to correct for this difference, for instance
     * in augmented reality applications.
     *
     * The mechanic works like this: instead of injecting one RSM, two a rendered. One
     * is rendered with a scene, the other with the same scene and an additional object.
     * By injecting the latter first, and then injecting the former negatively, the
     * delta is extracted and propagated in the volume.
     *
     * A DLPV also injects direct light to form out rough shadow blobs. Propagation
     * of direct and indirect injects is independent from one another.
     */
    class delta_light_propagation_volume : public light_propagation_volume
    {
    protected:
        ID3D11BlendState*       bs_negative_inject_;
        ID3D11VertexShader*     vs_direct_inject_;

        bool negative_;

        struct cbs_delta_injection
        {
            FLOAT scale;
            BOOL is_direct;
            FLOAT pad1;
            FLOAT pad2;
        };

        cbuffer<cbs_delta_injection> cb_delta_injection_;

    public:
        delta_light_propagation_volume();
        virtual ~delta_light_propagation_volume() {}

        virtual void create(ID3D11Device* device, UINT volume_size);
        virtual void destroy();

        /*! \brief Additionally to the regular indirect injection shader, this will set the direct injection pixel shader. */
        void set_direct_inject_shader(ID3D11VertexShader* ps);

        /*!
         * \brief Overwrites the default injection to include  direct injects.
         *
         * After each clear, RSMs are injected in a toggled order: positive, negative, positive, negative ...
         *
         * \param context A Direct3D context.
         * \param rsm The GBuffer of the RSM.
         * \param clear Set to true if the injected volume should be cleared. If you inject multiple RSMs set this only at the first call to true.
         * \param is_direct Specify if this is a direct or indirect injection.
         * \param scale A multiplier for injected VPL intensity
         */
        void inject(ID3D11DeviceContext* context, gbuffer& rsm, bool clear, bool is_direct, float scale);

        /*! \brief Render/propagate indirect injects. */
        void render_indirect(ID3D11DeviceContext* context);

        //!@{
        /*! \brief Render/propagate direct injects. */
        void propagate_direct(ID3D11DeviceContext* context, size_t num_iterations);
        void render_direct(ID3D11DeviceContext* context, size_t num_iterations, UINT lpv_out_start_slot);
        //!@}
    };
}

#endif
