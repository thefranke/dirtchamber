/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "d3d_tools.h"

#include <iostream>
#include <cassert>
#include <algorithm>

#include "unicode.h"
#include "exception.h"

namespace dune
{
    void assert_hr_detail(const HRESULT& hr, const char* file, DWORD line, const char* msg)
    {
    #if defined(DEBUG) || defined(_DEBUG)
    #endif

        if (FAILED(hr))
        {
            tstringstream ss;
            ss << "Error in " << file << " (" << line << "): " << msg << std::endl;

            throw exception(ss.str());
        }
    }

    profile_query::profile_query() :
        frequency_(nullptr),
        start_(nullptr),
        stop_(nullptr),
        context_(nullptr)
    {
    }

    void profile_query::create(ID3D11Device* device)
    {
        D3D11_QUERY_DESC desc;
        desc.MiscFlags = 0;

        desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        assert_hr(device->CreateQuery(&desc, &frequency_));

        desc.Query = D3D11_QUERY_TIMESTAMP;
        assert_hr(device->CreateQuery(&desc, &start_));
        assert_hr(device->CreateQuery(&desc, &stop_));
    }

    void profile_query::destroy()
    {
        safe_release(frequency_);
        safe_release(start_);
        safe_release(stop_);
        context_ = nullptr;
    }

    void profile_query::begin(ID3D11DeviceContext* context)
    {
        context_ = context;

        context_->Begin(frequency_);
        context_->End(start_);
    }

    void profile_query::end()
    {
        context_->End(stop_);
        context_->End(frequency_);
    }

    float profile_query::result()
    {
        end();

        UINT64 start = 0;
        while(context_->GetData(start_, &start, sizeof(UINT64), 0) != S_OK);

        UINT64 stop = 0;
        while(context_->GetData(stop_, &stop, sizeof(UINT64), 0) != S_OK);

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
        while(context_->GetData(frequency_, &disjoint, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0) != S_OK);

        UINT64 time = stop - start;

        return (static_cast<float>(time)/disjoint.Frequency) * 1000.f;
    }

    bool is_srgb(ID3D11RenderTargetView* rtv)
    {
        D3D11_RENDER_TARGET_VIEW_DESC desc;
        rtv->GetDesc(&desc);

        return is_srgb(desc.Format);
    }

    bool is_srgb(DXGI_FORMAT f)
    {
        return f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
               f == DXGI_FORMAT_BC1_UNORM_SRGB ||
               f == DXGI_FORMAT_BC2_UNORM_SRGB ||
               f == DXGI_FORMAT_BC3_UNORM_SRGB ||
               f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
               f == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB ||
               f == DXGI_FORMAT_BC7_UNORM_SRGB;
    }

    void set_viewport(ID3D11DeviceContext* context, size_t w, size_t h)
    {
        D3D11_VIEWPORT viewport;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = static_cast<FLOAT>(w);
        viewport.Height = static_cast<FLOAT>(h);
        viewport.MinDepth = 0;
        viewport.MaxDepth = 1;
        context->RSSetViewports(1, &viewport);
    }

    DirectX::XMFLOAT3 max(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        DirectX::XMFLOAT3 r;

        r.x = std::max(a.x, b.x);
        r.y = std::max(a.y, b.y);
        r.z = std::max(a.z, b.z);

        return r;
    }

    DirectX::XMFLOAT3 min(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        DirectX::XMFLOAT3 r;

        r.x = std::min(a.x, b.x);
        r.y = std::min(a.y, b.y);
        r.z = std::min(a.z, b.z);

        return r;
    }

    bool equal(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b)
    {
        auto aa = DirectX::XMLoadFloat4(&a);
        auto bb = DirectX::XMLoadFloat4(&b);

        return DirectX::XMVector4Equal(aa, bb);
    }

    void clear_rtvs(ID3D11DeviceContext* context, ID3D11RenderTargetView** rtvs, size_t num_rtvs, FLOAT* clear_color)
    {
        std::for_each(rtvs, rtvs+num_rtvs,
            [&context, &clear_color](ID3D11RenderTargetView* rtv)
            {
                context->ClearRenderTargetView(rtv, clear_color);
            }
        );
    }
}
