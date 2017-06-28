/*
 * Dune D3D library - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_SPARSE_VOXEL_OCTREE
#define DUNE_SPARSE_VOXEL_OCTREE

#include <D3D11.h>

#include "cbuffer.h"
#include "shader_resource.h"

namespace dune
{
    struct d3d_mesh;
    class gilga_mesh;
    class directional_light;
    class differential_directional_light;
}

namespace dune
{
    /*!
     * \brief A sparse voxel octree (SVO).
     *
     * This class implements a sparse voxel octree (currently just a volume) filtered and uploaded to the GPU, where it can
     * be used for Voxel Cone Tracing. The SVO contains to volume textures: v_normal_, which contains a four component 3D texture
     * where each texel is an RGB encoded normal plus a occlusion marker, and v_rho_ which contains the injected first bounce
     * from an RSM.
     *
     * A typical SVO is handled in three steps when rendering:
     * - One or more calls to voxelize meshes
     * - One or more calls to inject RSMs
     * - A final call to filter (i.e. mip-map) both volume textures
     *
     * After uploading both volumes to the GPU, voxel cone tracing can be performed.
     */
    class sparse_voxel_octree : public shader_resource
    {
    public:
        struct param
        {
            float gi_scale;
            float glossiness;
            UINT num_vpls;
            BOOL debug_gi;
        };

    protected:
        ID3D11BlendState*           bs_voxelize_;
        ID3D11SamplerState*         ss_visualize_;

        ID3D11VertexShader*         vs_voxelize_;
        ID3D11GeometryShader*       gs_voxelize_;
        ID3D11PixelShader*          ps_voxelize_;

        ID3D11VertexShader*         vs_inject_;
        ID3D11PixelShader*          ps_inject_;

        INT                         inject_rsm_rho_start_slot_;

        ID3D11Texture3D*            v_normal_;
        ID3D11Texture3D*            v_rho_;

        ID3D11ShaderResourceView*   srv_v_normal_;
        ID3D11ShaderResourceView*   srv_v_rho_;

        ID3D11UnorderedAccessView*  uav_v_normal_;
        ID3D11UnorderedAccessView*  uav_v_rho_;

        ID3D11RasterizerState*      no_culling_;

        UINT                        volume_size_;

        DirectX::XMFLOAT4X4         world_to_svo_;
        DirectX::XMFLOAT3           svo_min_, svo_max_;

        struct cbs_parameters
        {
            DirectX::XMFLOAT4X4 world_to_svo;
            DirectX::XMFLOAT4   bb_min;
            DirectX::XMFLOAT4   bb_max;
        };

        cbuffer<cbs_parameters> cb_parameters_;

        dune::profile_query profiler_;

        typedef cbuffer<param> cb_gi_parameters;
        cb_gi_parameters cb_gi_parameters_;

        UINT last_bound_;

    protected:
        virtual void clear_ps(ID3D11DeviceContext* context);

    public:
        float time_voxelize_;
        float time_inject_;
        float time_mip_;

    public:
        sparse_voxel_octree();
        virtual ~sparse_voxel_octree() {}

        //!@{
        /*! \brief Return local cbuffer parameters. */
        cb_gi_parameters& parameters() { return cb_gi_parameters_; }
        const cb_gi_parameters& parameters() const { return cb_gi_parameters_; }
        //!@}

        virtual void create(ID3D11Device* device, UINT volume_size);
        virtual void destroy();

        /*!
         * \brief Set the complete voxelization shader.
         *
         * This method sets the voxelization shader, which includes a vertex, geometry and pixel shader.
         * The shader is an implementation of [[Schwarz and Seidel 2010]](http://research.michael-schwarz.com/publ/2010/vox/).
         *
         * \param vs The voxelization vertex shader.
         * \param gs The voxelization geometry shader.
         * \param ps The voxelization pixel shader.
         */
        void set_shader_voxelize(ID3D11VertexShader* vs, ID3D11GeometryShader* gs, ID3D11PixelShader* ps);

        /*!
         * \brief Set the complete injection shader.
         *
         * This method sets the injection shader, which includes a vertex and pixel shader, and a slot
         * at which register the shader can expect to read a gbuffer of the RSM with colors, normals and
         * lineardepth.
         *
         * \param vs The injection vertex shader.
         * \param ps The injection pixel shader.
         * \param rsm_start_slot The slot at which the gbuffer of an RSM starts.
         */
        void set_shader_inject(ID3D11VertexShader* vs, ID3D11PixelShader* ps, UINT rsm_start_slot);

        /*! \brief Voxelize a mesh into a volume with normals and an occupied marker. If clear is true, the volume is cleared before voxelization. */
        void voxelize(ID3D11DeviceContext* context, gilga_mesh& mesh, bool clear = true);

        /*! \brief Inject a bounce from directional_light into the SVO. */
        virtual void inject(ID3D11DeviceContext* context, directional_light& light);

        /*! \brief Pre-filter (i.e. mip-map) the SVO. */
        void filter(ID3D11DeviceContext* context);

        //!@{
        /*! \brief Set/get the world -> SVO matrix, which transforms world coordinates to SVO volume coordinates. */
        void set_model_matrix(ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& model, const DirectX::XMFLOAT3& svo_min, const DirectX::XMFLOAT3& svo_max, UINT svo_parameters_slot);
        const DirectX::XMFLOAT4X4& world_to_svo() const { return world_to_svo_; }
        //!@}

        virtual void to_ps(ID3D11DeviceContext* context, UINT volume_start_slot);
    };

    /*!
     * \brief A Delta Radiance Field of an SVO necessary for Delta Voxel Cone Tracing (DVCT)
     *
     * This class is an implementation of [[Franke 2014]](http://www.tobias-franke.eu/publications/franke14dvct).
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
    class delta_sparse_voxel_octree : public sparse_voxel_octree
    {
    protected:
        ID3D11Texture3D*            v_delta_;
        ID3D11ShaderResourceView*   srv_v_delta_;
        ID3D11UnorderedAccessView*  uav_v_delta_;

        INT                         inject_rsm_mu_start_slot_;

    protected:
        virtual void clear_ps(ID3D11DeviceContext* context);

    public:
        delta_sparse_voxel_octree();
        virtual ~delta_sparse_voxel_octree() {}

        virtual void create(ID3D11Device* device, UINT volume_size);
        virtual void destroy();

        void set_shader_inject(ID3D11VertexShader* vs, ID3D11PixelShader* ps, UINT mu_start_slot, UINT rho_start_slot);

        virtual void inject(ID3D11DeviceContext* context, differential_directional_light& light);
        virtual void to_ps(ID3D11DeviceContext* context, UINT volume_start_slot);
    };
}

#endif
