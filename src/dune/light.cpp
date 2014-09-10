/* 
 * dune::light by Tobias Alexander Franke (tob@cyberhead.de) 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "light.h"
#include "d3d_tools.h"

#include <cassert>

namespace dune
{
    void directional_light::update(const DirectX::XMFLOAT4& upt, const DirectX::XMMATRIX& light_proj)
    {
        DirectX::XMVECTOR up = DirectX::XMLoadFloat4(&upt);
        DirectX::XMVECTOR eye = DirectX::XMLoadFloat4(&parameters().data().position);
        DirectX::XMVECTOR dir = DirectX::XMLoadFloat4(&parameters().data().direction);

        // guard against bogus directions
        if (parameters().data().direction.x + parameters().data().direction.y + parameters().data().direction.z == 0)
            return;

        DirectX::XMMATRIX light_view = DirectX::XMMatrixLookToLH(eye, dir, up);

        DirectX::XMMATRIX light_view_inv = DirectX::XMMatrixInverse(nullptr, light_view);

        DirectX::XMMATRIX vp = light_view * light_proj;
        DirectX::XMStoreFloat4x4(&cb_param_.data().vp, vp);

        DirectX::XMMATRIX vp_inv = DirectX::XMMatrixInverse(nullptr, vp);
        DirectX::XMStoreFloat4x4(&cb_param_.data().vp_inv, vp_inv);

        DirectX::XMMATRIX tex
        (
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.f, 0.f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.5f, 1.0f
        );

        DirectX::XMStoreFloat4x4(&cb_param_.data().vp_tex, vp * tex);
    }

    void directional_light::create(ID3D11Device* device)
    {
        assert(device);
        cb_param_.create(device);
    }

    void directional_light::create(ID3D11Device* device, const tstring& name, const DXGI_SURFACE_DESC& colors, const DXGI_SURFACE_DESC& normals, const DXGI_SURFACE_DESC& depth)
    {
        create(device);
        rsm_.create(device, name);

        // setup reflective shadow map
        rsm_.add_target(L"lineardepth", depth, 0);
        rsm_.add_target(L"colors", colors);
        rsm_.add_target(L"normals", normals);
    }

    void directional_light::create(ID3D11Device* device, const tstring& name, const D3D11_TEXTURE2D_DESC& colors, const D3D11_TEXTURE2D_DESC& normals, const D3D11_TEXTURE2D_DESC& depth)
    {
        create(device);
        rsm_.create(device, name);

        // setup reflective shadow map
        rsm_.add_target(L"lineardepth", depth);
        rsm_.add_target(L"colors", colors);
        rsm_.add_target(L"normals", normals);
    }

    void directional_light::prepare_render(ID3D11DeviceContext* context)
    {
        static float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };

        ID3D11RenderTargetView* rsm_views[] = { 
            rsm_[L"colors"]->rtv(),
            rsm_[L"normals"]->rtv(),
            rsm_[L"lineardepth"]->rtv()
        };

        clear_rtvs(context, rsm_views, 3, clear_color);

	    DirectX::XMFLOAT2 size = rsm_[L"colors"]->size();
        set_viewport(context,
                        static_cast<size_t>(size.x), 
                        static_cast<size_t>(size.y));
    }

    gbuffer& directional_light::rsm()
    {
        return rsm_;
    }

    void directional_light::destroy()
    {
        rsm_.destroy();
        cb_param_.destroy();
    }

    void differential_directional_light::create(ID3D11Device* device, const tstring& name, const D3D11_TEXTURE2D_DESC& colors, const D3D11_TEXTURE2D_DESC& normals, const D3D11_TEXTURE2D_DESC& depth)
    {
        directional_light::create(device, name + L"_rho", colors, normals, depth);

        // setup reflective shadow map
        mu_.create(device, name + L"_mu");
        mu_.add_target(L"lineardepth", depth);
        mu_.add_target(L"colors", colors);
        mu_.add_target(L"normals", normals);
    }

    void differential_directional_light::destroy()
    {
        directional_light::destroy();
        mu_.destroy();
    }
}