/* 
 * dune::mesh by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "mesh.h"

//#include "sdk_mesh.h"
#include "assimp_mesh.h"
#include "d3d_tools.h"
#include "unicode.h"

namespace dune 
{
    void load_model(ID3D11Device* device, const tstring& file, std::shared_ptr<d3d_mesh>& ptr)
    {
        tstring path = file;

	    // TODO
	    /*
        bool is_sdkmesh = path.find(L".sdkmesh") != std::string::npos;

        if (is_sdkmesh)
            ptr.reset(new sdk_mesh());
        else*/
            ptr.reset(new gilga_mesh());

        ptr->create(device, file);
    }

    d3d_mesh::d3d_mesh() :
        vs_(nullptr),
        ps_(nullptr),
        vertex_layout_(nullptr),
        diffuse_tex_slot_(-1),
        normal_tex_slot_(-1),
        specular_tex_slot_(-1)
    {
    }

    void d3d_mesh::destroy()
    {
        safe_release(vertex_layout_);
        safe_release(ps_);
        safe_release(vs_);
        diffuse_tex_slot_ = -1;
        specular_tex_slot_ = -1;
        normal_tex_slot_ = -1;
    }

    UINT count(const D3D11_INPUT_ELEMENT_DESC* desc)
    {
        UINT array_size = 0;
        while (desc->SemanticName != nullptr) { array_size++; desc++; }
        return array_size;
    }

    void d3d_mesh::set_shader(ID3D11Device* device, ID3DBlob* input_binary, ID3D11VertexShader* vs, ID3D11PixelShader* ps)
    {
        exchange(&vs_, vs);
        exchange(&ps_, ps);

        const D3D11_INPUT_ELEMENT_DESC* vd = vertex_desc();
    
        UINT array_size = count(vd);

        safe_release(vertex_layout_);

        assert_hr(device->CreateInputLayout(vd, static_cast<UINT>(array_size), input_binary->GetBufferPointer(),
                                            input_binary->GetBufferSize(), &vertex_layout_));
    }

    bool is_visible(const aabb<DirectX::XMFLOAT3>& bbox, const DirectX::XMFLOAT4X4& to_clip)
    {
	    DirectX::XMFLOAT3 mi = bbox.bb_min();
	    DirectX::XMFLOAT3 ma = bbox.bb_max();

	    DirectX::XMFLOAT3 aabb[8] = 
	    {
		    mi, ma, 
		    DirectX::XMFLOAT3(mi.x, ma.y, ma.z),
		    DirectX::XMFLOAT3(mi.x, mi.y, ma.z),
		    DirectX::XMFLOAT3(mi.x, ma.y, mi.z),
		    DirectX::XMFLOAT3(ma.x, mi.y, mi.z),
		    DirectX::XMFLOAT3(ma.x, mi.y, ma.z),
		    DirectX::XMFLOAT3(ma.x, ma.y, mi.z),
	    };

        DirectX::XMVECTOR aabb_trans[8];

	    DirectX::XMMATRIX m_to_clip = DirectX::XMLoadFloat4x4(&to_clip);
            
        for (size_t i = 0; i < 8; ++i)
		    aabb_trans[i] = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&aabb[i]), m_to_clip);

        bool inside;

        for (size_t i = 0; i < 6; ++i)
        {    
            for (size_t k = 0; k < 3; ++k)
            {
                inside = false;

                for (size_t j = 0; j < 8; ++j)
                {
				    if (DirectX::XMVectorGetByIndex(aabb_trans[j], k) > -DirectX::XMVectorGetW(aabb_trans[j]))
                    {
                        inside = true;
                        break;
                    }
                }

                if (!inside) break;

                inside = false;

                for (size_t j = 0; j < 8; ++j)
                {
				    if (DirectX::XMVectorGetByIndex(aabb_trans[j], k) < DirectX::XMVectorGetW(aabb_trans[j]))
                    {
                        inside = true;
                        break;
                    }
                }

                if (!inside) break;
            }

            if (!inside) break;
        }

        return inside;
    }
} 
