/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_SIMPLE_MESH
#define DUNE_SIMPLE_MESH

#include <D3D11.h>
#include <vector>

#include "d3d_tools.h"
#include "mesh.h"

namespace dune
{
    /*!
     * \brief A very simple mesh class with position-only vertex attributes.
     *
     * This class represents a very simple implementation of a d3d_mesh. A simple_mesh only contains vertices with a position attribute.
     * Use this class if you want to specify simple geometry for which fixed attributes will be declared in a shader. An example usage
     * of this class is the fullscreen triangle used for a deferred renderer.
     *
     * \note Yes I know you can use SV_VertexID as well to generate a fullscreen triangle, but PIX didn't like it and since then I didn't care...
     *
     */
    class simple_mesh : public d3d_mesh
    {
    protected:
        struct vertex
        {
            DirectX::XMFLOAT3 v;
        };

        std::vector<vertex> vertices_;

        ID3D11Buffer* mesh_data_;

    protected:
        virtual const D3D11_INPUT_ELEMENT_DESC* vertex_desc()
        {
            static D3D11_INPUT_ELEMENT_DESC l[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                0
            };

            return l;
        }

    public:
        simple_mesh() :
            vertices_(),
            mesh_data_()
        {
        }

        virtual ~simple_mesh() {}

        /*! \brief Add a new vertex p to the mesh. */
        void push_back(DirectX::XMFLOAT3& p)
        {
            vertex v = { p };
            vertices_.push_back(v);
        }

        /*!
         * \brief Create a new simple_mesh.
         *
         * This function creates a new, empty simple_mesh.
         *
         * \param device The Direct3D device.
         * \param name Instead of a filename, a name is used to write the creating of a simple_mesh into a log. This parameter serves no other purpose.
         */
        virtual void create(ID3D11Device* device, const tstring& name)
        {
            tclog << L"Creating: simple geometry " << name << L" with " << num_faces() << " faces" << std::endl;

            D3D11_BUFFER_DESC bd;
            ZeroMemory(&bd, sizeof(bd));
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(vertex)* 3;
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA InitData;
            ZeroMemory(&InitData, sizeof(InitData));
            InitData.pSysMem = &vertices_[0];

            assert_hr(device->CreateBuffer(&bd, &InitData, &mesh_data_));
        }

        virtual void render(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* to_clip = nullptr)
        {
            context->VSSetShader(vs_, nullptr, 0);
            context->IASetInputLayout(vertex_layout_);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            static const UINT stride = sizeof(vertex);
            static const UINT offset = 0;

            context->IASetVertexBuffers(0, 1, &mesh_data_, &stride, &offset);

            context->Draw(static_cast<UINT>(num_vertices()), 0);
        }

        virtual size_t num_vertices()
        {
            return vertices_.size();
        }

        virtual size_t num_faces()
        {
            return num_vertices() / 3;
        }

        virtual void destroy()
        {
            safe_release(mesh_data_);
            vertices_.clear();

            d3d_mesh::destroy();
        }
    };
}

#endif
