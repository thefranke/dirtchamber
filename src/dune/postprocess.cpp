/*
 * Dune D3D library - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "postprocess.h"
#include "d3d_tools.h"

namespace dune
{
    postprocessor::postprocessor() :
        buffers_start_slot_(-1),
        fs_triangle_(),
        frontbuffer_(),
        enabled_(false)
    {
    }

    void postprocessor::set_shader(ID3D11Device* device, ID3DBlob* input_binary, ID3D11VertexShader* fs_triangle, UINT buffers_start_slot)
    {
        fs_triangle_.set_shader(device, input_binary, fs_triangle, nullptr);

        buffers_start_slot_ = buffers_start_slot;

        do_set_shader(device);
    }

    void postprocessor::create(ID3D11Device* device, DXGI_SURFACE_DESC desc)
    {
        disable();

        fs_triangle_.push_back(DirectX::XMFLOAT3(-1.f, -3.f, 1.f));
        fs_triangle_.push_back(DirectX::XMFLOAT3(-1.f, 1.f, 1.f));
        fs_triangle_.push_back(DirectX::XMFLOAT3(3.f, 1.f, 1.f));

        fs_triangle_.create(device, L"full_screen_tri");
        frontbuffer_.create(device, desc, 0);
        do_create(device);

        enable();
    }

    void postprocessor::render(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer)
    {

    }

    void postprocessor::destroy()
    {
        do_destroy();
        frontbuffer_.destroy();
        fs_triangle_.destroy();
        buffers_start_slot_ = -1;
    }

    void postprocessor::resize(UINT width, UINT height)
    {
        frontbuffer_.resize(width, height);
        do_resize(width, height);
    }
}
