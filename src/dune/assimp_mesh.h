/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_ASSIMP_MESH
#define DUNE_ASSIMP_MESH

#include <vector>
#include <map>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "mesh.h"
#include "cbuffer.h"

namespace dune
{
    /*!
     * \brief A d3d_mesh created with the help of Assimp.
     *
     * This is the base interface class for a d3d_mesh loading a file via Assimp.
     * When the model is loaded, each single vertex of the model is copied over into a
     * vertex structure and then handed to a method push_back(). Because
     * this method is virtual, derived classes from assimp_mesh may handle vertex
     * data differently. For instance, a PRT loader may discard several vertex attributes
     * which another loader might not. In any case, the push_back() method is responsible
     * to put the vertex into a structure that will later be sent to the GPU.
     */
    class assimp_mesh : public d3d_mesh
    {
    public:
        /*! \brief A vertex with all its attributes as loaded by Assimp. */
        struct vertex
        {
            DirectX::XMFLOAT3 position;
            DirectX::XMFLOAT3 normal;
            DirectX::XMFLOAT3 tangent;
            DirectX::XMFLOAT4 color;
            DirectX::XMFLOAT2 texcoord[AI_MAX_NUMBER_OF_TEXTURECOORDS];
        };

    protected:
        /*! \brief A helper struct to supply information about submeshes loaded by Assimp. */
        struct mesh_info : public aabb<DirectX::XMFLOAT3>
        {
            UINT vstart_index;
            UINT istart_index;
            UINT num_faces;
            UINT num_vertices;
            UINT material_index;

            void update(const DirectX::XMFLOAT3& v, bool first)
            {
                if (first)
                    init_bb(v);
                else
                    update_bb(v);
            }
        };

    protected:
        Assimp::Importer importer_;
        std::vector<mesh_info> mesh_infos_;
        std::vector<unsigned int> indices_;

    private:
        // warning: do not confuse with functions num_vertices() and num_faces()
        UINT num_faces_;
        UINT num_vertices_;

    protected:
        /*! \brief Push back a new vertex: this member needs to be overloaded in derived assimp_mesh objects. */
        virtual void push_back(vertex v) = 0;

        virtual void load_internal(const aiScene* scene, aiNode* node, aiMatrix4x4 transform);

        void load(const tstring& file);

    public:
        assimp_mesh();
        virtual ~assimp_mesh() {};

        const aiScene* const assimp_scene() const;
        virtual void destroy();

        size_t num_vertices();
        size_t num_faces();
    };

    /*! \brief The default layout for a gigla_mesh. */
    const D3D11_INPUT_ELEMENT_DESC gilgamesh_vertex_desc[5] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,      0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        0
    };

    /*!
     * \brief Default implementation of assimp_mesh (pun intended).
     *
     * This class is the default implementation for models loaded with an assimp_mesh. Vertices
     * transported from assimp_mesh via push_back are internally copied into gilga_vertex vertices
     * and uploaded to the GPU.
     */
    class gilga_mesh : public assimp_mesh
    {
    protected:
        /*! \brief gilga_mesh supports different shading modes to switch lighting. */
        enum
        {
            SHADING_OFF,
            SHADING_DEFAULT,
            SHADING_IBL,
            SHADING_AREA_LIGHT
        };

        /*! \brief A gilga_vertex comes with position, normal, one texture coordinate and a tanget. */
        struct gilga_vertex
        {
            DirectX::XMFLOAT3 position;
            DirectX::XMFLOAT3 normal;
            DirectX::XMFLOAT2 texcoord;
            DirectX::XMFLOAT3 tangent;
        };

        /*! \brief Mesh constant buffer data uploaded to the vertex shader. */
        struct mesh_data_vs
        {
            DirectX::XMFLOAT4X4 world;
        };

        /*! \brief Mesh constant buffer data uploaded to the pixel shader. */
        struct mesh_data_ps
        {
            DirectX::XMFLOAT4 diffuse_color;
            DirectX::XMFLOAT4 specular_color;
            DirectX::XMFLOAT4 emissive_color;
            BOOL has_diffuse_tex;
            BOOL has_normal_tex;
            BOOL has_specular_tex;
            BOOL has_alpha_tex;
            UINT shading_mode;
            FLOAT roughness;
            FLOAT refractive_index;
            FLOAT pad;
        };

        /*! \brief Mesh data for CPU side useage. */
        struct mesh_data
        {
            ID3D11Buffer* index;
            ID3D11Buffer* vertex;
            DirectX::XMFLOAT4 diffuse_color;
            DirectX::XMFLOAT4 specular_color;
            DirectX::XMFLOAT4 emissive_color;
            ID3D11ShaderResourceView* diffuse_tex;
            ID3D11ShaderResourceView* emissive_tex;
            ID3D11ShaderResourceView* specular_tex;
            ID3D11ShaderResourceView* normal_tex;
            ID3D11ShaderResourceView* alpha_tex;
            UINT shading_mode;
            FLOAT roughness;
            FLOAT refractive_index;
        };

    protected:
        cbuffer<mesh_data_ps> cb_mesh_data_ps_;
        cbuffer<mesh_data_vs> cb_mesh_data_vs_;

        sampler_state ss_;

        INT alpha_tex_slot_;

        std::vector<gilga_vertex> vertices_;
        std::vector<mesh_data> meshes_;

    protected:
        void push_back(vertex v);

        virtual const D3D11_INPUT_ELEMENT_DESC* vertex_desc() { return gilgamesh_vertex_desc; }

    public:
        gilga_mesh();
        virtual ~gilga_mesh() {};

        virtual void create(ID3D11Device* device, const tstring& file);
        virtual void render(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* to_clip = nullptr);
        virtual void destroy();

        /*! \brief Prepares the context for rendering a gilga_mesh with the correct shader. */
        void prepare_context(ID3D11DeviceContext* context);

        /*! \brief Render the gilga_mesh without touching the current state, which is useful if the shader has been set externally. */
        void render_direct(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* to_clip);

        /*!
         * \brief Set the texture register for alpha textures.
         *
         * gilga_mesh objects can have alpha textures, hence this function can be
         * used to set the texture register for a texture.
         *
         * \param alpha_tex The register of the alpha texture. Leaving the value at -1 is equal to ignoring the texture slot.
         */
        void set_alpha_slot(INT alpha_tex = -1)
        {
            alpha_tex_slot_ = alpha_tex;
        }
    };
}

#endif
