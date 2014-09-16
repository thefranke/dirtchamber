/* 
 * Postprocessing pipeline by Tobias Alexander Franke (tob@cyberhead.de) 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "pppipe.h"

#include "idc.h"

#include <iostream>
#include <cassert>

#include <dune/d3d_tools.h>
#include <dune/shader_tools.h>

#include <D3Dcompiler.h>

#define FRONTBUFFER_SLOT         0
#define ADAPTED_LUMINANCE_SLOT   1
#define BLOOM_BUFFER_SLOT        2
#define FRONTBUFFER_BLURRED_SLOT 3

using namespace dune;

void pppipe::do_set_shader(ID3D11Device* device)
{
    DWORD shader_flags = D3DCOMPILE_ENABLE_STRICTNESS;
    
#if defined(DEBUG) | defined(_DEBUG)
    shader_flags |= D3DCOMPILE_DEBUG;
#endif
 
    auto check = [&](const TCHAR* f, const char* n, ID3D11PixelShader** ps){ compile_shader(device, f, "ps_5_0", n, shader_flags, nullptr, ps); };

    check(L"../../shader/pp_ssao.hlsl", "ps_ssao", &ssao_);
    check(L"../../shader/pp_godrays.hlsl", "ps_godrays_halfres", &godrays_);
    check(L"../../shader/pp_godrays.hlsl", "ps_godrays_merge", &godrays_merge_);
    check(L"../../shader/pp_bloom.hlsl", "ps_bloom", &bloom_);
    check(L"../../shader/pp_dof.hlsl", "ps_depth_of_field", &dof_);
    check(L"../../shader/pp_bloom.hlsl", "ps_adapt_exposure", &adapt_exposure_);
    check(L"../../shader/pp_fxaa.hlsl", "ps_fxaa", &fxaa_);
    check(L"../../shader/pp_blur.hlsl", "ps_gauss_bloom_v", &gauss_bloom_v_);
    check(L"../../shader/pp_blur.hlsl", "ps_gauss_bloom_h", &gauss_bloom_h_);
    check(L"../../shader/pp_blur.hlsl", "ps_gauss_dof_v", &gauss_dof_v_);
    check(L"../../shader/pp_blur.hlsl", "ps_gauss_dof_h", &gauss_dof_h_);
    check(L"../../shader/pp_blur.hlsl", "ps_gauss_godrays_v", &gauss_godrays_v_);
    check(L"../../shader/pp_blur.hlsl", "ps_gauss_godrays_h", &gauss_godrays_h_);
    check(L"../../shader/pp_blur.hlsl", "ps_copy", &copy_);
    check(L"../../shader/pp_bloom.hlsl", "ps_bloom_treshold", &bloom_treshold_);
    check(L"../../shader/pp_crt.hlsl", "ps_crt", &crt_);
    check(L"../../shader/pp_film_grain.hlsl", "ps_film_grain", &film_grain_);
}

void pppipe::do_resize(UINT width, UINT height)
{
    temporary_.resize(width, height);
    frontbuffer_blurred_.resize(width, height);
}

void pppipe::do_create(ID3D11Device* device)
{
    // create backbuffer with as many mipmaps as possible    
    auto desc = frontbuffer_.desc();
    temporary_.create(device, desc.Width, desc.Height, desc.Format, desc.SampleDesc);
    frontbuffer_blurred_.create(device, desc.Width, desc.Height, desc.Format, desc.SampleDesc);
    cb_pp_parameters_.create(device);

    DXGI_SAMPLE_DESC msaa;
    msaa.Count = 1;
    msaa.Quality = 0;

    rt_adapted_luminance_[0].create(device, 1, 1, DXGI_FORMAT_R32_FLOAT, msaa);
    rt_adapted_luminance_[1].create(device, 1, 1, DXGI_FORMAT_R32_FLOAT, msaa);

    bloom_full_.create(device, desc.Width, desc.Height, desc.Format, desc.SampleDesc);
    
    blurred_[0].create(device, desc.Width/2,  desc.Height/2,  desc.Format, desc.SampleDesc);
    blurred_[5].create(device, desc.Width/2,  desc.Height/2,  desc.Format, desc.SampleDesc);

    blurred_[1].create(device, desc.Width/4,  desc.Height/4,  desc.Format, desc.SampleDesc);
    blurred_[2].create(device, desc.Width/8,  desc.Height/8,  desc.Format, desc.SampleDesc);
    blurred_[3].create(device, desc.Width/16, desc.Height/16, desc.Format, desc.SampleDesc);
    blurred_[4].create(device, desc.Width/16, desc.Height/16, desc.Format, desc.SampleDesc);
}

void pppipe::do_destroy()
{
    rt_adapted_luminance_[0].destroy();
    rt_adapted_luminance_[1].destroy();

    temporary_.destroy();

    for (size_t i = 0; i < 6; ++i)
        blurred_[i].destroy();

    bloom_full_.destroy();
    frontbuffer_blurred_.destroy();

    safe_release(ssao_);
    safe_release(bloom_);
    safe_release(godrays_);
    safe_release(dof_);
    safe_release(adapt_exposure_);
    safe_release(fxaa_);
    safe_release(bloom_treshold_);
    safe_release(gauss_bloom_h_);
    safe_release(gauss_bloom_v_);
    safe_release(gauss_dof_h_);
    safe_release(gauss_dof_v_);
    safe_release(gauss_godrays_h_);
    safe_release(gauss_godrays_v_);
    safe_release(godrays_merge_);
    safe_release(copy_);
    safe_release(crt_);
    safe_release(film_grain_);

    cb_pp_parameters_.destroy();
}

void pppipe::render(ID3D11DeviceContext* context, ID3D11PixelShader* ps, render_target& in, ID3D11RenderTargetView* out)
{
    ID3D11ShaderResourceView* views[] = { in.srv() };
    context->PSSetShaderResources(buffers_start_slot_ + FRONTBUFFER_SLOT, 1, views);
    
    context->OMSetRenderTargets(1, &out, nullptr);
    context->PSSetShader(ps, nullptr, 0);
   
    fs_triangle_.render(context);

    context->OMSetRenderTargets(0, nullptr, nullptr);

    ID3D11ShaderResourceView* nulls[] = { nullptr };
    context->PSSetShaderResources(buffers_start_slot_ + FRONTBUFFER_SLOT, 1, nulls);
}

void set_viewport(ID3D11DeviceContext* context, render_target& target)
{
    set_viewport(context, target.desc().Width, target.desc().Height);
}

void pppipe::dof(ID3D11DeviceContext* context, render_target& in, render_target& out)
{
    // render blurred backbuffer
	if (cb_pp_parameters_.data().dof_enabled)
		dofblur(context, in, frontbuffer_blurred_);

	ID3D11ShaderResourceView* v[] = { frontbuffer_blurred_.srv()  };
    ID3D11ShaderResourceView* n[] = { nullptr };
    
    context->PSSetShaderResources(buffers_start_slot_ + FRONTBUFFER_BLURRED_SLOT, 1, v);

    render(context, dof_, in, out.rtv());

    context->PSSetShaderResources(buffers_start_slot_ + FRONTBUFFER_BLURRED_SLOT, 1, n);
}

void pppipe::dofblur(ID3D11DeviceContext* context, render_target& in, render_target& out)
{
	// bind frontbuffer input for depth checked DOF
	ID3D11ShaderResourceView* views1[] = { in.srv() };
    context->PSSetShaderResources(0, 1, views1);

    // downscale
    set_viewport(context, blurred_[0]);
    render(context, copy_, in, blurred_[0].rtv());
    
    for (size_t i = 0; i < 2; ++i)
    {
        // apply v-blur
        render(context, gauss_dof_v_, blurred_[0], blurred_[5].rtv());
    
        // apply h-blur
        render(context, gauss_dof_h_, blurred_[5], blurred_[0].rtv());
    }

    // upscale
    set_viewport(context, out);
    render(context, copy_, blurred_[0], out.rtv());

	// unbind again so we can use it
	ID3D11ShaderResourceView* nv[] = { nullptr };
	context->PSSetShaderResources(0, 1, nv);
}

void pppipe::bloomblur(ID3D11DeviceContext* context, render_target& in, render_target& out)
{
    // downscale
    set_viewport(context, blurred_[0]);
    render(context, copy_, in, blurred_[0].rtv());
    set_viewport(context, blurred_[1]);
    render(context, copy_, blurred_[0], blurred_[1].rtv());
    set_viewport(context, blurred_[2]);
    render(context, copy_, blurred_[1], blurred_[2].rtv());
    set_viewport(context, blurred_[3]);
    render(context, copy_, blurred_[2], blurred_[3].rtv());

    for (size_t i = 0; i < 4; ++i)
    {
        // apply v-blur
        render(context, gauss_bloom_v_, blurred_[3], blurred_[4].rtv());
    
        // apply h-blur
        render(context, gauss_bloom_h_, blurred_[4], blurred_[3].rtv());
    }

    // upscale
    set_viewport(context, blurred_[2]);
    render(context, copy_, blurred_[3], blurred_[2].rtv());
    set_viewport(context, blurred_[1]);
    render(context, copy_, blurred_[2], blurred_[1].rtv());
    set_viewport(context, blurred_[0]);
    render(context, copy_, blurred_[1], blurred_[0].rtv());
    set_viewport(context, out);
    render(context, copy_, blurred_[0], out.rtv());
}

void pppipe::bloom(ID3D11DeviceContext* context, render_target& frontbuffer)
{
    set_viewport(context, bloom_full_);

    // apply treshold
    render(context, bloom_treshold_, frontbuffer, bloom_full_.rtv());
    bloomblur(context, bloom_full_, bloom_full_);    
}

void pppipe::godrays(ID3D11DeviceContext* context, dune::render_target& in, dune::render_target& out)
{
    if (cb_pp_parameters_.data().godrays_enabled)
    {

        // downscale
        set_viewport(context, blurred_[0]);
        render(context, copy_, in, blurred_[0].rtv());

        // render godrays
        render(context, godrays_, blurred_[0], blurred_[5].rtv());

        // bind frontbuffer input for bilateral blur
        ID3D11ShaderResourceView* views1[] = { nullptr };
        context->PSSetShaderResources(0, 1, views1);

        for (size_t i = 0; i < 2; ++i)
        {
            // apply v-blur
            render(context, gauss_godrays_v_, blurred_[5], blurred_[0].rtv());

            // apply h-blur
            render(context, gauss_godrays_h_, blurred_[0], blurred_[5].rtv());
        }

        // upscale
        set_viewport(context, out);
        render(context, copy_, blurred_[5], frontbuffer_blurred_.rtv());
    }

    // bind frontbuffer_blurred
    ID3D11ShaderResourceView* v[] = { frontbuffer_blurred_.srv() };
    ID3D11ShaderResourceView* n[] = { nullptr };

    context->PSSetShaderResources(buffers_start_slot_ + FRONTBUFFER_BLURRED_SLOT, 1, v);

    // merge with image
    render(context, godrays_merge_, in, out.rtv());

    context->PSSetShaderResources(buffers_start_slot_ + FRONTBUFFER_BLURRED_SLOT, 1, n);
}

void pppipe::render(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer)
{   
    assert(context);
    assert(backbuffer);

    context->GenerateMips(frontbuffer_.srv());

    // adapt exposure
    static bool current = true;
    current = !current;

    set_viewport(context, 1, 1);
    
    ID3D11ShaderResourceView* views0[] = { rt_adapted_luminance_[!current].srv() };
    context->PSSetShaderResources(buffers_start_slot_ + ADAPTED_LUMINANCE_SLOT, 1, views0);

    // render target luminance to buffer to adapt exposure
    render(context, adapt_exposure_, frontbuffer_, rt_adapted_luminance_[current].rtv());

    // render bloom buffer
	if (parameters().data().bloom_enabled)
		bloom(context, frontbuffer_);

    ID3D11ShaderResourceView* views3[] = { rt_adapted_luminance_[current].srv(), bloom_full_.srv() };
    context->PSSetShaderResources(buffers_start_slot_ + ADAPTED_LUMINANCE_SLOT, 2, views3);

    set_viewport(context, frontbuffer_);

	render_target* in  = &frontbuffer_;
	render_target* out = &temporary_;

	// ssao
    render(context, ssao_, *in, out->rtv());
	std::swap(in, out);

	// dof
    dof(context, *in, *out);
	std::swap(in, out);

    // godrays
    godrays(context, *in, *out);
	std::swap(in, out);

	// bloom + tonemap + gamma correction
	render(context, bloom_, *in, out->rtv());
	std::swap(in, out);

    // crt
    render(context, crt_, *in, out->rtv());
    std::swap(in, out);

	// fxaa
    render(context, fxaa_, *in, out->rtv());
	std::swap(in, out);

    // tv
    render(context, film_grain_, *in, backbuffer);
    std::swap(in, out);

    // reset
    ID3D11ShaderResourceView* nulls[] = { nullptr, nullptr, nullptr, nullptr };
    context->PSSetShaderResources(buffers_start_slot_ + FRONTBUFFER_SLOT, 4, nulls);
}

serializer& operator<<(serializer& s, const pppipe& ppp)
{
    s.put(L"postprocessing.bloom.enabled",      ppp.parameters().data().bloom_enabled);
    s.put(L"postprocessing.bloom.sigma",        ppp.parameters().data().bloom_sigma);
    s.put(L"postprocessing.bloom.treshold",     ppp.parameters().data().bloom_treshold);

    s.put(L"postprocessing.crt.enabled",        ppp.parameters().data().crt_enabled);
    s.put(L"postprocessing.film_grain.enabled", ppp.parameters().data().film_grain_enabled);

    s.put(L"postprocessing.dof.enabled",        ppp.parameters().data().dof_enabled);
    s.put(L"postprocessing.dof.coc_scale",      ppp.parameters().data().dof_coc_scale);
    s.put(L"postprocessing.dof.focal_plane",    ppp.parameters().data().dof_focal_plane);

    s.put(L"postprocessing.exposure.adapt",     ppp.parameters().data().exposure_adapt);
    s.put(L"postprocessing.exposure.key",       ppp.parameters().data().exposure_key);
    s.put(L"postprocessing.exposure.speed",     ppp.parameters().data().exposure_speed);

    s.put(L"postprocessing.fxaa.enabled",       ppp.parameters().data().fxaa_enabled);

    s.put(L"postprocessing.godrays.enabled",    ppp.parameters().data().godrays_enabled);
    s.put(L"postprocessing.godrays.tau",        ppp.parameters().data().godrays_tau);

    s.put(L"postprocessing.ssao.enabled",       ppp.parameters().data().ssao_enabled);
    s.put(L"postprocessing.ssao.scale",         ppp.parameters().data().ssao_scale);

    return s;
}

const serializer& operator>>(const serializer& s, pppipe& ppp)
{
    try
    {
        ppp.parameters().data().bloom_enabled       = s.get<bool>(L"postprocessing.bloom.enabled");
        ppp.parameters().data().bloom_sigma         = s.get<float>(L"postprocessing.bloom.sigma");
        ppp.parameters().data().bloom_treshold      = s.get<float>(L"postprocessing.bloom.treshold");

        ppp.parameters().data().crt_enabled         = s.get<bool>(L"postprocessing.crt.enabled");
        ppp.parameters().data().film_grain_enabled  = s.get<bool>(L"postprocessing.film_grain.enabled");

        ppp.parameters().data().dof_enabled         = s.get<bool>(L"postprocessing.dof.enabled");
        ppp.parameters().data().dof_coc_scale       = s.get<float>(L"postprocessing.dof.coc_scale");
        ppp.parameters().data().dof_focal_plane     = s.get<float>(L"postprocessing.dof.focal_plane");

        ppp.parameters().data().exposure_adapt      = s.get<bool>(L"postprocessing.exposure.adapt");
        ppp.parameters().data().exposure_key        = s.get<float>(L"postprocessing.exposure.key");
        ppp.parameters().data().exposure_speed      = s.get<float>(L"postprocessing.exposure.speed");

        ppp.parameters().data().fxaa_enabled        = s.get<bool>(L"postprocessing.fxaa.enabled");

        ppp.parameters().data().godrays_enabled     = s.get<bool>(L"postprocessing.godrays.enabled");
        ppp.parameters().data().godrays_tau         = s.get<float>(L"postprocessing.godrays.tau");

        ppp.parameters().data().ssao_enabled        = s.get<bool>(L"postprocessing.ssao.enabled");
        ppp.parameters().data().ssao_scale          = s.get<float>(L"postprocessing.ssao.scale");
    }
    catch (dune::exception& e)
    {
        tcerr << "Couldn't load postprocessing pipe parameters: " << e.msg() << std::endl;
    }

    return s;
}
