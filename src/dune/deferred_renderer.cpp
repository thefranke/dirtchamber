/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "deferred_renderer.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>

#include "gbuffer.h"
#include "postprocess.h"
#include "d3d_tools.h"
#include "unicode.h"
#include "mesh.h"
#include "exception.h"
#include "common_tools.h"
#include "record_tools.h"

namespace dune
{
    namespace detail
    {
        static video_recorder recorder;
    }

    #define MAX_TEXTURE_SLOTS 14

    deferred_renderer::deferred_renderer() :
        buffers_(),
        srvs_(),
        buffers_slot_(-1),
        overlay_slot_(-1),
        overlay_(nullptr),
        fs_triangle_(),
        ps_deferred_(nullptr),
        ps_overlay_(nullptr),
        ss_shadows_(),
        ss_lpv_(),
        postprocessor_(nullptr),
        do_record_(false)
    {}

    void deferred_renderer::start_capture(ID3D11Device* device, size_t width, size_t height, size_t fps)
    {
        do_record_ = true;

        detail::recorder.create(device, static_cast<UINT>(width), static_cast<UINT>(height), static_cast<UINT>(fps));
        detail::recorder.start_recording();
    }

    void deferred_renderer::stop_capture()
    {
        do_record_ = false;
        detail::recorder.destroy();
    }

    void deferred_renderer::blit(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer)
    {
        assert(context);
        assert(backbuffer);

        context->OMSetRenderTargets(1, &backbuffer, nullptr);

        fs_triangle_.render(context);
    }

    void deferred_renderer::render(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer, ID3D11DepthStencilView* dsv)
    {
        deferred(context, backbuffer);
        overlay(context, backbuffer);
    }

    void deferred_renderer::deferred(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer)
    {
        reassign_everything(context);

        ss_lpv_.to_ps(context, 1);
        ss_shadows_.to_ps(context, 2);

        context->GSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(ps_deferred_, nullptr, 0);

        if (postprocessor_)
        {
            blit(context, postprocessor_->frontbuffer().rtv());

            context->OMSetRenderTargets(0, nullptr, nullptr);
            postprocessor_->render(context, backbuffer);
        }
        else
            blit(context, backbuffer);

        if (do_record_)
            detail::recorder.add_frame(context, backbuffer);

        // reset everything
        context->OMSetRenderTargets(1, &backbuffer, nullptr);
    }

    void deferred_renderer::set_postprocessor(postprocessor* pp)
    {
        postprocessor_ = pp;
    }

    void deferred_renderer::overlay(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer)
    {
        if (overlay_)
        {
            ID3D11ShaderResourceView* views[] = { overlay_ };
            context->PSSetShaderResources(overlay_slot_, 1, views);

            context->GSSetShader(nullptr, nullptr, 0);
            context->PSSetShader(ps_overlay_, nullptr, 0);

            blit(context, backbuffer);
        }

        clear_assigned(context);
    }

    void deferred_renderer::destroy()
    {
        ss_shadows_.destroy();
        ss_lpv_.destroy();

        safe_release(ps_deferred_);
        safe_release(ps_overlay_);

        buffers_.clear();
        srvs_.clear();
        fs_triangle_.destroy();

        buffers_slot_ = -1;
        overlay_slot_ = -1;

        stop_capture();

        postprocessor_ = nullptr;
        overlay_ = nullptr;
    }

    void deferred_renderer::set_shader(ID3D11Device* device,
                                       ID3DBlob* input_binary,
                                       ID3D11VertexShader* vs_deferred,
                                       ID3D11PixelShader* ps_deferred,
                                       ID3D11PixelShader* ps_overlay,
                                       UINT start_slot,
                                       UINT overlay_slot)
    {
        exchange(&ps_deferred_, ps_deferred);
        exchange(&ps_overlay_, ps_overlay);

        buffers_slot_ = start_slot,
        overlay_slot_ = overlay_slot;

        fs_triangle_.set_shader(device, input_binary, vs_deferred, nullptr);
    }

    void deferred_renderer::create(ID3D11Device* device)
    {
        assert(device);

        overlay_ = 0;

        D3D11_SAMPLER_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        ss_shadows_.create(device, sd);

        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.BorderColor[0] = sd.BorderColor[1] = sd.BorderColor[2] = sd.BorderColor[3] = 0.f;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.MaxAnisotropy = 1;
        ss_lpv_.create(device, sd);

        srvs_.resize(MAX_TEXTURE_SLOTS);
        std::fill_n(srvs_.begin(), MAX_TEXTURE_SLOTS, nullptr);

        fs_triangle_.push_back(DirectX::XMFLOAT3(-1.f, -3.f, 1.f));
        fs_triangle_.push_back(DirectX::XMFLOAT3(-1.f,  1.f, 1.f));
        fs_triangle_.push_back(DirectX::XMFLOAT3( 3.f,  1.f, 1.f));

        fs_triangle_.create(device, L"fullscreen_tri");
    }

    ID3D11ShaderResourceView* deferred_renderer::target(const tstring& name) const
    {
        assert(name != L"");

        if (name == L"overlay_tex")
            return overlay_;

        for (auto &b : buffers_)
            if (b.second->name == name)
                return b.second->srv();

        return 0;
    }

    void deferred_renderer::show_target(ID3D11DeviceContext* context, const tstring& name)
    {
        overlay_ = target(name);
    }

    void deferred_renderer::resize(UINT width, UINT height)
    {
        // gather srvs anew
        std::fill_n(srvs_.begin(), MAX_TEXTURE_SLOTS, nullptr);

        for (auto &b : buffers_)
            srvs_[b.first] = b.second->srv();

        if (postprocessor_)
            postprocessor_->resize(width, height);
    }

    void deferred_renderer::reassign_everything(ID3D11DeviceContext* context)
    {
        for (auto &b : buffers_)
            b.second->to_ps(context, b.first);
    }

    void deferred_renderer::clear_assigned(ID3D11DeviceContext* context)
    {
        static ID3D11ShaderResourceView* null_views[] =
        {
            nullptr
        };

        for (auto &b : buffers_)
            context->PSSetShaderResources(b.first, 1, null_views);

        if (overlay_)
            context->PSSetShaderResources(overlay_slot_, 1, null_views);
    }

    void deferred_renderer::add_buffer(gbuffer& buffer, UINT slot)
    {
        for (auto &target : buffer.targets())
            add_buffer(target, slot++);
    }

    void deferred_renderer::add_buffer(texture& target, UINT slot)
    {
        if (slot > MAX_TEXTURE_SLOTS-1)
            tcerr << L"Can't add buffer: Only " << MAX_TEXTURE_SLOTS << " slots available" << std::endl;

        tclog << L"Adding target to renderer: " << target.name.c_str() << std::endl;

        buffers_[slot] = &target;
        srvs_[slot] = target.srv();
    }

    const deferred_renderer::buffer_collection& deferred_renderer::buffers()
    {
        return buffers_;
    }
}
