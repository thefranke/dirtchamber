/*
 * Dune D3D library - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "shader_resource.h"

#include "unicode.h"
#include "d3d_tools.h"

namespace dune
{
    void shader_resource::to_vs(ID3D11DeviceContext* context, UINT slot)
    {
        tcerr << name.c_str() << L" has no to_vs implementation" << std::endl;
    }

    void shader_resource::to_gs(ID3D11DeviceContext* context, UINT slot)
    {
        tcerr << name.c_str() << L" has no to_gs implementation" << std::endl;
    }

    void shader_resource::to_ps(ID3D11DeviceContext* context, UINT slot)
    {
        tcerr << name.c_str() << L" has no to_ps implementation" << std::endl;
    }

    void shader_resource::to_cs(ID3D11DeviceContext* context, UINT slot)
    {
        tcerr << name.c_str() << L" has no to_cs implementation" << std::endl;
    }

    void sampler_state::create(ID3D11Device* device, const D3D11_SAMPLER_DESC& desc)
    {
        assert_hr(device->CreateSamplerState(&desc, &state));
    }

    void sampler_state::destroy()
    {
        safe_release(state);
    }

    void sampler_state::to_vs(ID3D11DeviceContext* context, UINT slot)
    {
        context->VSSetSamplers(slot, 1, &state);
    }

    void sampler_state::to_ps(ID3D11DeviceContext* context, UINT slot)
    {
        context->PSSetSamplers(slot, 1, &state);
    }
}
