/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_MESH
#define DUNE_MESH

#include <D3D11.h>
#include <DirectXMath.h>

#include <memory>
#include <cassert>

#include "unicode.h"

namespace dune
{
    /*!
     * \brief An axis-aligned bounding-box.
     *
     * This class represents an axis-aligned bounding-box.
     */
    template<typename V>
    struct aabb
    {
    protected:
        V bb_max_;
        V bb_min_;
        DirectX::XMFLOAT4X4 world_;

    protected:
        /*!
         * \brief Initialize the bounding-box with a first point.
         *
         * Before using the bounding box, it must first be intialized with a single point.
         *
         * \param p The first point of a larger structure.
         */
        inline void init_bb(const V& p)
        {
            bb_max_ = bb_min_ = p;
        }

        /*!
         * \brief Update the bounding-box with a new point.
         *
         * Updates the bounding-box with an additional point and recalculates its
         * maximum and minimum extents.
         *
         * \param p A new point.
         */
        void update_bb(const V& p)
        {
            bb_max_ = max(p, bb_max_);
            bb_min_ = min(p, bb_min_);
        }

    public:
        /*! \brief Returns the center of the bounding-box. */
        inline V center() const { return bb_min_ + (bb_max_ - bb_min_)/2.0f; };

        /*! \brief Returns the maximum (upper/right/back) of the bounding-box. */
        inline V bb_max() const { return bb_max_; };

        /*! \brief Returns the minimum (lower/left/front) of the bounding-box. */
        inline V bb_min() const { return bb_min_; };

        /*! \brief Update the world matrix of this mesh. */
        inline virtual void set_world(const DirectX::XMFLOAT4X4& world)
        {
            world_ = world;
        }

        /*! \brief Returns the current world matrix of this mesh. */
        inline const DirectX::XMFLOAT4X4& world()
        {
            return world_;
        }
    };

    /*!
     * \brief Check if a bounding-box is visible given a clipping matrix.
     *
     * This function determines if a bounding box is partially or fully visible, given a clip-space matrix.
     *
     * \param bbox The bounding-box object.
     * \param to_clip A clip-space matrix.
     * \return True if bbox is partially or fully visible.
     */
    bool is_visible(const aabb<DirectX::XMFLOAT3>& bbox, const DirectX::XMFLOAT4X4& to_clip);

    /*!
     * \brief Basic interface of a mesh.
     *
     * A mesh is a bounding box with functions to determine the number of faces and vertices in it.
     */
    template<typename V>
    struct mesh : public aabb<V>
    {
    public:
        /*! \brief Returns the number of vertices of a mesh. */
        virtual size_t num_vertices() = 0;

        /*! \brief Returns the number of faces of a mesh. */
        virtual size_t num_faces() = 0;
    };

    /*! \brief The standard layout of a d3d_mesh. */
    const D3D11_INPUT_ELEMENT_DESC standard_vertex_desc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,      0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        0
    };

    /*!
     * \brief Main D3D mesh interface class.
     *
     * This class represents the base interface of a Direct3D mesh. A Direct3D mesh needs at least a vertex and a pixel shader,
     * a vertex layout descriptor and shader registers to upload diffuse, normal and specular textures. Each mesh object has
     * a set_shader function to set input layout, vertex and pixel shader from outside. Each mesh object additionally has a
     * set_shader_slots function to externally control the register slots to look for texture objects when rendering a mesh.
     */
    struct d3d_mesh : public mesh<DirectX::XMFLOAT3>
    {
    protected:
        ID3D11VertexShader* vs_;
        ID3D11PixelShader* ps_;

        ID3D11InputLayout* vertex_layout_;

        INT diffuse_tex_slot_;
        INT normal_tex_slot_;
        INT specular_tex_slot_;

    protected:
        /* !\brief Returns the vertex_descriptor of a basic d3d_mesh. */
        virtual const D3D11_INPUT_ELEMENT_DESC* vertex_desc() { return standard_vertex_desc; }

    public:
        d3d_mesh();
        virtual ~d3d_mesh() {}

        /*! \brief Sets the input layout, vertex- and pixel shader of this mesh. */
        virtual void set_shader(ID3D11Device* device, ID3DBlob* input_binary, ID3D11VertexShader* vs, ID3D11PixelShader* ps);

        /*! \brief Sets three register numbers to identify the slots the pixel shader is looking for surface textures. */
        virtual void set_shader_slots(INT diffuse_tex = -1,
                                      INT normal_tex = -1,
                                      INT specular_tex = -1)
        {
            diffuse_tex_slot_  = diffuse_tex;
            normal_tex_slot_   = normal_tex;
            specular_tex_slot_ = specular_tex;
        }

        /*! \brief Create a mesh from a given filename. */
        virtual void create(ID3D11Device* device, const tstring& filename) = 0;

        /*! \brief Destroy a mesh and free all its resources. */
        virtual void destroy();

        /*!
         * \brief Renders the mesh using the shaders previously set.
         *
         * When calling render, the mesh will be rendered using the shaders set with set_shaders(). Additionally, a clip-space
         * matrix can be set to clip away unseen geometry.
         *
         * \param context A Direct3D context.
         * \param to_clip A clip-space matrix against which the mesh is being culled.
         */
        virtual void render(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* to_clip = nullptr) = 0;
    };

    /*!
     * \brief Load a model specified by a filename.
     *
     * This is the principal loading function for model data. A model is loaded from the disk and
     * the correct loader to do so is determined by the file's extension. If loding the model was
     * successful, a shared_ptr to a d3d_mesh is set.
     *
     * \param device The Direct3D device.
     * \param file A string of a filename on the disk.
     * \param ptr A shared_ptr to a d3d_mesh which will contain the model once successfully loaded.
     */
    void load_model(ID3D11Device* device, const tstring& file, std::shared_ptr<d3d_mesh>& ptr);
}

#endif // MESH
