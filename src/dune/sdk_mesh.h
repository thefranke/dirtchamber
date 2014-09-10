/* 
 * dune::sdk_mesh by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_SDK_MESH
#define DUNE_SDK_MESH

#include <memory>

#include "mesh.h"
#include "cbuffer.h"

class CDXUTSDKMesh;

namespace dune {

const D3D11_INPUT_ELEMENT_DESC sdkmesh_vertex_desc[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,      0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    0
};

/*! \brief A d3d_mesh created with the SDKMesh DXUT loader. */
class sdk_mesh : public d3d_mesh
{
protected:
    std::shared_ptr<CDXUTSDKMesh> sdk_mesh_;

    struct cbs_mesh_data_ps
    {
        DirectX::XMFLOAT4 diffuse_color;
        BOOL has_diffuse_tex;
        BOOL has_normal_tex;
        BOOL has_specular_tex;
        BOOL has_alpha_tex;
    };

    struct cbs_mesh_data_vs
    {
        DirectX::XMFLOAT4X4 world;
    };

    cbuffer<cbs_mesh_data_ps> cb_mesh_data_ps_;
    cbuffer<cbs_mesh_data_vs> cb_mesh_data_vs_;

protected:
    virtual void reset();
    virtual const D3D11_INPUT_ELEMENT_DESC* vertex_desc() { return sdkmesh_vertex_desc; }
	void load_texture(ID3D11Device* device, ID3D11Resource* texture, ID3D11ShaderResourceView* srv, void* context);

public:
    size_t num_vertices();
    size_t num_faces();

    void create(ID3D11Device* device, LPCTSTR file);
    virtual void render(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* to_clip);
};

} 

#endif // SDK_MESH