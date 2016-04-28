/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_COMPOSITE_MESH
#define DUNE_COMPOSITE_MESH

#include "mesh.h"

#include <memory>
#include <vector>

namespace dune
{
    /*!
     * \brief A mesh composed from other meshes.
     *
     * A composite_mesh object is simply a collection of other d3d_mesh objects.
     * It can be used to group up objects together or simply have one representative
     * for many loaded meshes.
     *
     * This is not a scenegraph node!
     */
    class composite_mesh : public d3d_mesh
    {
    protected:
        typedef std::shared_ptr<d3d_mesh> mesh_ptr;
        std::vector<mesh_ptr> meshes_;

    public:
        size_t num_vertices();
        size_t num_faces();

        virtual void set_world(const DirectX::XMFLOAT4X4& world);

        void set_shader(ID3D11Device* device, ID3DBlob* input_binary, ID3D11VertexShader* vs, ID3D11PixelShader* ps);

        virtual void set_shader_slots(INT diffuse_tex = -1, INT specular_tex = -1, INT normal_tex = -1);

        void create(ID3D11Device* device, const tstring& file);

        /*!
         * \brief Create a composite_mesh from a filename pattern.
         *
         * Calling this function will search a directory for a pattern and load all
         * hits into as separate meshes to group them up in a composite_mesh.
         *
         * \param device The Direct3D device.
         * \param pattern A string representing a file pattern, e.g. C:/models/\*.obj.
         *
         */
        void create_from_dir(ID3D11Device* device, const tstring& pattern);

        /*! \brief Add a new mesh m to the composite_mesh. */
        void push_back(mesh_ptr& m);
        void render(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* to_clip = nullptr);

        /*! \brief Returns the mesh at index i. */
        mesh_ptr operator[](size_t i);

        /*! \brief Returns the number of submeshes. */
        inline size_t size() { return meshes_.size(); }

        virtual void destroy();
    };
}

#endif // COMPOSITE_MESH
