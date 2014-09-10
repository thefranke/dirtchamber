/* 
 * dune::sdk_mesh by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "sdk_mesh.h"

#include <iostream>

#include <DXUT.h>
#include <SDKmesh.h>

#include "texture_cache.h"
#include "d3d_tools.h"

#include "unicode.h"
#include "common_tools.h"

namespace dune {

void CALLBACK load_texture_cb(ID3D11Device* device, char* texture_file, ID3D11ShaderResourceView** srv, void* context)
{
    tstring* base_path = reinterpret_cast<tstring*>(context);

	load_texture(device, (*base_path + to_tstring(texture_file)), srv);
}

HRESULT sdk_mesh::create(ID3D11Device* device, LPCTSTR file)
{
    HRESULT hr;

    sdk_mesh_.reset(new CDXUTSDKMesh);

    tstring path = extract_path(file);

	SDKMESH_CALLBACKS11 callbacks;
	callbacks.pCreateTextureFromFile = &dune::load_texture_cb;
	callbacks.pCreateVertexBuffer = nullptr;
	callbacks.pCreateIndexBuffer = nullptr;
	callbacks.pContext = reinterpret_cast<void*>(&path);

    V_RETURN(sdk_mesh_->Create(device, file, false, &callbacks));

    V_RETURN(cb_mesh_data_vs_.create(device));
    V_RETURN(cb_mesh_data_ps_.create(device));

    init_bb(sdk_mesh_->GetMeshBBoxCenter(0));

    for (UINT i = 0; i < sdk_mesh_->GetNumMeshes(); ++i)
    {
        update_bb(sdk_mesh_->GetMeshBBoxCenter(i) - sdk_mesh_->GetMeshBBoxExtents(i));
        update_bb(sdk_mesh_->GetMeshBBoxCenter(i) + sdk_mesh_->GetMeshBBoxExtents(i));
    }
        
    return S_OK;
}

void sdk_mesh::reset()
{
    sdk_mesh_->Destroy();

    cb_mesh_data_ps_.destroy();
    cb_mesh_data_vs_.destroy();
}

void sdk_mesh::render(ID3D11DeviceContext* context, D3DXMATRIX* to_clip)
{
    D3D11_PRIMITIVE_TOPOLOGY PrimType = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

    assert(context);
    context->IASetInputLayout(vertex_layout_);
    
    context->VSSetShader(vs_, nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(ps_, nullptr, 0);
    
    //context->PSSetSamplers(0, 1, &ss_);

    context->OMSetBlendState(nullptr, nullptr, 0xFF);

    auto cbvs = cb_mesh_data_vs_.map(context);
    {
        D3DXMatrixTranspose(&cbvs->world, &world);
    }
    cb_mesh_data_vs_.unmap_vs(1);

    #define MAX_D3D11_VERTEX_STREAMS D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT

    if(sdk_mesh_->GetOutstandingBufferResources())
        return;

	//sdk_mesh_->RenderMesh(0, false, context, diffuse_tex_slot_, normal_tex_slot_, specular_tex_slot_);

    for(UINT m = 0; m < sdk_mesh_->GetNumMeshes(); m++)
    {
        SDKMESH_MESH* mesh = sdk_mesh_->GetMesh(m);

        UINT strides[MAX_D3D11_VERTEX_STREAMS];
        UINT offsets[MAX_D3D11_VERTEX_STREAMS];
        ID3D11Buffer* vb[MAX_D3D11_VERTEX_STREAMS];

        if(mesh->NumVertexBuffers > MAX_D3D11_VERTEX_STREAMS)
            return;

        for(UINT i = 0; i < mesh->NumVertexBuffers; ++i)
        {
            vb[i] = sdk_mesh_->GetVB11(m, i);
            strides[i] = sdk_mesh_->GetVertexStride(m, i);
            offsets[i] = 0;
        }

        ID3D11Buffer* index_buffer = sdk_mesh_->GetIB11(m);
        DXGI_FORMAT format = sdk_mesh_->GetIBFormat11(m);
    
        context->IASetVertexBuffers(0, mesh->NumVertexBuffers, vb, strides, offsets);
        context->IASetIndexBuffer(index_buffer, format, 0);

        for(UINT s = 0; s < mesh->NumSubsets; s++)
        {
            SDKMESH_SUBSET* subset = sdk_mesh_->GetSubset(m, s);
		
            auto pt = sdk_mesh_->GetPrimitiveType11(static_cast<SDKMESH_PRIMITIVE_TYPE>(subset->PrimitiveType));
            context->IASetPrimitiveTopology(pt);

            SDKMESH_MATERIAL* material = sdk_mesh_->GetMaterial(subset->MaterialID);

            auto cb = cb_mesh_data_ps_.map(context);
            {
                cb->diffuse_color    = material->Diffuse;
                cb->has_diffuse_tex  = material->pDiffuseRV11  != nullptr && diffuse_tex_slot_  != -1 && !IsErrorResource(material->pDiffuseRV11);
                cb->has_normal_tex   = material->pNormalRV11   != nullptr && normal_tex_slot_   != -1 && !IsErrorResource(material->pNormalRV11);
                cb->has_specular_tex = material->pSpecularRV11 != nullptr && specular_tex_slot_ != -1 && !IsErrorResource(material->pSpecularRV11);
                cb->has_alpha_tex    = false;
            }
            cb_mesh_data_ps_.unmap_ps(0);

            if (cb->has_diffuse_tex)
                context->PSSetShaderResources(diffuse_tex_slot_, 1, &material->pDiffuseRV11);

            if (cb->has_normal_tex)
                context->PSSetShaderResources(normal_tex_slot_, 1, &material->pNormalRV11);

            if (cb->has_specular_tex)
                context->PSSetShaderResources(specular_tex_slot_, 1, &material->pSpecularRV11);

            UINT index_count  = static_cast<UINT>(subset->IndexCount);
            UINT index_start  = static_cast<UINT>(subset->IndexStart);
            UINT vertex_start = static_cast<UINT>(subset->VertexStart);
        
            context->DrawIndexed(index_count, index_start, vertex_start);
        }
    }

}

size_t sdk_mesh::num_vertices()
{
    size_t v = 0;

    for (UINT i = 0; i < sdk_mesh_->GetNumMeshes(); ++i)
        v += static_cast<size_t>(sdk_mesh_->GetNumVertices(i, 0));

    return v;
}

size_t sdk_mesh::num_faces()
{
    size_t ind = 0;
    
    for (UINT i = 0; i < sdk_mesh_->GetNumMeshes(); ++i)
        ind += static_cast<size_t>(sdk_mesh_->GetNumIndices(i));

    return ind;
}

} 
