/*
 * Dune D3D library - Tobias Alexander Franke 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "camera.h"

namespace dune
{
    void camera::create(ID3D11Device* device)
    {
        cb_param_.create(device);
    }

    void camera::destroy()
    {
        cb_param_.destroy();
    }

    void camera::update(float z_far)
    {
        DirectX::XMMATRIX view = GetViewMatrix();
        DirectX::XMMATRIX proj = GetProjMatrix();

        DirectX::XMStoreFloat4x4(&parameters().data().vp, view * proj);

        DirectX::XMFLOAT4X4 temp;
        DirectX::XMStoreFloat4x4(&temp, view);

        temp._41 = 0;
        temp._42 = 0;
        temp._43 = 0;

        DirectX::XMMATRIX no_translate = DirectX::XMLoadFloat4x4(&temp);

        DirectX::XMMATRIX vpp = no_translate * proj;

        DirectX::XMStoreFloat4x4(&parameters().data().vp_inv, DirectX::XMMatrixInverse(nullptr, vpp));

        DirectX::XMStoreFloat3(&parameters().data().camera_pos, GetEyePt());
        parameters().data().z_far = z_far;
    }
}
