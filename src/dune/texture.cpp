/*
* dune::texture by Tobias Alexander Franke (tob@cyberhead.de) 2011
* For copyright and license see LICENSE
* http://www.tobias-franke.eu
*/

#include "texture.h"

#include "d3d_tools.h"
#include "exception.h"

#include <iostream>
#include <list>
#include <algorithm> 

namespace dune
{
    texture::texture() :
        srview_(nullptr),
        texture_(nullptr),
        size_(0.f, 0.f)
    {
    }

    void texture::create(ID3D11Device* device, const DXGI_SURFACE_DESC& desc, UINT num_mipmaps)
    {
        create(device, desc.Width, desc.Height, desc.Format, desc.SampleDesc, num_mipmaps);
    }

    void texture::create(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format, const DXGI_SAMPLE_DESC& msaa, UINT num_mipmaps)
    {
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleDesc = msaa;

        // TODO: Strange behavior?
        if (num_mipmaps == 0)
        {
            double dim = std::max(desc.Width, desc.Height);
            num_mipmaps = static_cast<UINT>(std::log(dim) / std::log(2.0));
        }

        desc.MipLevels = num_mipmaps;

        if (desc.MipLevels != 1)
            desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

        desc.Usage = D3D11_USAGE_DEFAULT;

        create(device, desc);
    }

    void texture::create(ID3D11Device* device, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA* subresource)
    {
        do_create(device, desc, subresource);
    }

    void texture::do_create(ID3D11Device* device, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA* subresource)
    {
        // TODO: MSAA in desc needs to be evaluated!

        size_ = DirectX::XMFLOAT2(static_cast<FLOAT>(desc.Width), static_cast<FLOAT>(desc.Height));

        desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
 
        assert_hr(device->CreateTexture2D(&desc, subresource, &texture_));

        D3D11_SHADER_RESOURCE_VIEW_DESC desc_srv;

        if (desc.Format == DXGI_FORMAT_R32_TYPELESS)
            desc_srv.Format = DXGI_FORMAT_R32_FLOAT;
        else
            desc_srv.Format = desc.Format;

        if (desc.ArraySize > 1)
        {
            desc_srv.Texture2DArray.ArraySize = desc.ArraySize;
            desc_srv.Texture2DArray.MipLevels = desc.MipLevels;
            desc_srv.Texture2DArray.FirstArraySlice = 0;
            desc_srv.Texture2DArray.MostDetailedMip = 0;
            desc_srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        }
        else
        {
            desc_srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            desc_srv.Texture2D.MostDetailedMip = 0;
            desc_srv.Texture2D.MipLevels = (desc.MipLevels == 0) ? 6 : desc.MipLevels;
        }

        assert_hr(device->CreateShaderResourceView(texture_, &desc_srv, &srview_));
    }

    void texture::destroy()
    {
        safe_release(texture_);
        safe_release(srview_);
        size_ = DirectX::XMFLOAT2(0.f, 0.f);
    }

    ID3D11Texture2D* texture::resource() const
    {
        return texture_;
    }

    void* texture::map(ID3D11DeviceContext* context)
    {
        if (!texture_)
            return nullptr;

        D3D11_MAPPED_SUBRESOURCE mapped_tex;

        if (FAILED(context->Map(texture_, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_WRITE_DISCARD, 0, &mapped_tex)))
            return nullptr;

        return mapped_tex.pData;
    }

    void texture::unmap(ID3D11DeviceContext* context)
    {
        context->Unmap(texture_, D3D11CalcSubresource(0, 0, 1));
    }

    DirectX::XMFLOAT2 texture::size() const
    {
        return size_;
    }

    D3D11_TEXTURE2D_DESC texture::desc() const
    {
        D3D11_TEXTURE2D_DESC desc;
        texture_->GetDesc(&desc);
        return desc;
    }

    void texture::to_vs(ID3D11DeviceContext* context, UINT slot)
    {
        ID3D11ShaderResourceView* v[] = { srv() };
        context->VSSetShaderResources(slot, 1, v);
    }

    void texture::to_ps(ID3D11DeviceContext* context, UINT slot)
    {
        ID3D11ShaderResourceView* v[] = { srv() };
        context->PSSetShaderResources(slot, 1, v);
    }
}
