/* 
 * dune::render_target by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "render_target.h"

#include "d3d_tools.h"
#include "exception.h"

#include "texture_cache.h"

namespace dune 
{
    render_target::render_target() :
        device_(nullptr),
        rtview_(nullptr),
        dsview_(nullptr),
        cached_(false),
        cached_copy_()
    {
    }

    void render_target::do_create(ID3D11Device* device, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA* subresource)
    {
        rtview_ = nullptr;
        dsview_ = nullptr;

        device_ = device;
        
        if (desc.Format == DXGI_FORMAT_R32_TYPELESS)
            desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

        if (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
        {
            tcout << "Target \"" << this->name.c_str() << "\" is technically not a render target but a depth stencil texture." << std::endl;
            
            texture::do_create(device, desc, subresource);

            D3D11_DEPTH_STENCIL_VIEW_DESC desc_dsv;
            ZeroMemory(&desc_dsv, sizeof(desc_dsv));
            desc_dsv.Format = DXGI_FORMAT_D32_FLOAT;
            desc_dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            desc_dsv.Flags = 0;
            desc_dsv.Texture2D.MipSlice = 0;

            assert_hr(device->CreateDepthStencilView(texture_, &desc_dsv, &dsview_));
        }
        else if (desc.Usage == D3D11_USAGE_DYNAMIC)
        {
            // target is dynamic and being "rendered" to from the outside via map/unmap/update/data()
            texture::do_create(device, desc, subresource);
        }
        else
        {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            texture::do_create(device, desc, subresource);
            assert_hr(device->CreateRenderTargetView(texture_, nullptr, &rtview_));
        }

        if (cached_)
        {
            if (subresource)
            {
                size_t num_bytes = desc.Height * subresource->SysMemPitch;
                cached_copy_.resize(num_bytes);
                copy(reinterpret_cast<const BYTE*>(subresource->pSysMem), num_bytes);
            }
            else
            {
                // TODO: fixme
                cached_copy_.resize(desc.Width * desc.Height * 4);
            }
        }
    }

    void render_target::resize(UINT width, UINT height)
    {
        if (width == size_.x &&
            height == size_.y)
            return;

        // get old format
        D3D11_TEXTURE2D_DESC desc;
        texture_->GetDesc(&desc);

        desc.Width = width;
        desc.Height = height;

        // save old device
        auto device = device_;

        destroy();
        create(device, desc);
    }

    void render_target::destroy()
    {
        texture::destroy();

        safe_release(rtview_);
        safe_release(dsview_);

        device_ = nullptr;
        cached_ = false;
        cached_copy_.clear();
    }

    void render_target::copy(const BYTE* data, size_t num_bytes)
    {
        std::memcpy(&cached_copy_[0], data, num_bytes);
    }

    void render_target::update(ID3D11DeviceContext* context)
    {
        void* p = map(context);

        if (p)
        {
            BYTE* target = reinterpret_cast<BYTE*>(p);
            std::memcpy(p, data<BYTE*>(), cached_copy_.size());
            unmap(context);
        }
    }

    void load_static_target(ID3D11Device* device, const tstring& filename, render_target& t)
    {
        t.enable_cached(true);
        load_texture(device, filename, t);
    }
} 
