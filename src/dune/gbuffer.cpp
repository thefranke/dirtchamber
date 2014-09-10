/* 
 * dune::gbuffer by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "gbuffer.h"

#include <cassert>
#include <algorithm>
#include <iostream>

#include "d3d_tools.h"

namespace dune 
{
    gbuffer::gbuffer() :
        srvs_(),
        buffers_(),
        device_(nullptr)
    {
    }

    void gbuffer::destroy()
    {
        std::for_each(buffers_.begin(), buffers_.end(), [](targets_vec::value_type& i){ i.destroy(); });
        buffers_.clear();
        srvs_.clear();
        device_ = nullptr;
    }

    render_target* gbuffer::operator[](const tstring& name)
    {
        return target(name);
    }

    const render_target* gbuffer::operator[](const tstring& name) const
    {
        return target(name);
    }

    gbuffer::targets_vec& gbuffer::targets()
    {
        return buffers_;
    }

    void gbuffer::resize(UINT width, UINT height)
    {
        srvs_.clear();
        std::for_each(buffers_.begin(), buffers_.end(), [&width, &height, this](targets_vec::value_type& rt){ rt.resize(width, height); srvs_.push_back(rt.srv()); });
    }

    render_target* gbuffer::target(const tstring& n)
    {
        assert(name != L"");

        const tstring full_name = name + L"_" + n;

        for (size_t i = 0; i < buffers_.size(); ++i)
            if (buffers_[i].name == full_name)
                return &buffers_[i];

        return 0;
    }

    const render_target* gbuffer::target(const tstring& n) const
    {
        assert(name != L"");

        const tstring full_name = name + L"_" + n;
    
        for (size_t i = 0; i < buffers_.size(); ++i)
            if (buffers_[i].name == full_name)
                return &buffers_[i];

        return 0;
    }

    void gbuffer::add_target(const tstring& n, render_target& t)
    {
        t.name = name + L"_" + n;
        buffers_.push_back(t);
        srvs_.push_back(t.srv());
    }

    void gbuffer::add_target(const tstring& n, const DXGI_SURFACE_DESC& desc, UINT num_mipmaps)
    {
        assert(device_);

        render_target t;
        t.create(device_, desc, num_mipmaps);
        t.name = this->name + L"_" + n;

        srvs_.push_back(t.srv());

        buffers_.push_back(t);
    }

    void gbuffer::add_target(const tstring& n, const D3D11_TEXTURE2D_DESC& desc, bool cached)
    {
        assert(device_);

        render_target t;
        t.enable_cached(cached);
        t.create(device_, desc);
        t.name = name + L"_" + n;
    
        srvs_.push_back(t.srv());

        buffers_.push_back(t);
    }

    void gbuffer::add_depth(const tstring& name, DXGI_SURFACE_DESC desc)
    {
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0; 
    
        assert(device_);

        add_target(name, desc);
    }

    void gbuffer::create(ID3D11Device* device, const tstring& n)
    {
        assert(device);

        device_ = device;
        name = n;

        buffers_.clear();
    }

    void gbuffer::to_ps(ID3D11DeviceContext* context, UINT start_slot)
    {
        context->PSSetShaderResources(start_slot, static_cast<UINT>(srvs_.size()), &srvs_[0]);
    }

    void gbuffer::to_gs(ID3D11DeviceContext* context, UINT start_slot)
    {
        context->GSSetShaderResources(start_slot, static_cast<UINT>(srvs_.size()), &srvs_[0]);
    }

    void gbuffer::to_vs(ID3D11DeviceContext* context, UINT start_slot)
    {
        context->VSSetShaderResources(start_slot, static_cast<UINT>(srvs_.size()), &srvs_[0]);
    }
} 
