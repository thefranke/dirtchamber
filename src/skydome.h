/*
 * The Dirtchamber - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#include <dune/texture_cache.h>
#include <dune/assimp_mesh.h>
#include <dune/d3d_tools.h>
#include <dune/common_tools.h>

/*!
 * \brief A simple skydome mesh.
 *
 * Load a skydome mesh, disable depth stencil and render it without transformation
 * and two sky-layers.
 */
class skydome : public dune::gilga_mesh
{
protected:
    ID3D11ShaderResourceView* envmap_;
    ID3D11ShaderResourceView* clouds_;

    ID3D11DepthStencilState* dss_disable_depth_test_;
    ID3D11DepthStencilState* dss_enable_depth_test_;

    // TODO: stencil depth test goes here!

public:
    virtual void create(ID3D11Device* device, const dune::tstring& file)
    {
        dune::gilga_mesh::create(device, dune::make_absolute_path(file).c_str());

        D3D11_DEPTH_STENCIL_DESC dsd;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsd.DepthFunc = D3D11_COMPARISON_LESS;
        dsd.StencilEnable = true;
        dsd.StencilReadMask = 0xFF;
        dsd.StencilWriteMask = 0xFF;
        dsd.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsd.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
        dsd.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        dsd.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        dsd.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsd.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
        dsd.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        dsd.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

        dsd.DepthEnable = FALSE;
        dune::assert_hr(device->CreateDepthStencilState(&dsd, &dss_disable_depth_test_));

        dsd.DepthEnable = TRUE;
        dune::assert_hr(device->CreateDepthStencilState(&dsd, &dss_enable_depth_test_));
    }

    virtual void destroy()
    {
        dune::gilga_mesh::destroy();

        dune::safe_release(dss_disable_depth_test_);
        dune::safe_release(dss_enable_depth_test_);
    }

    virtual void render(ID3D11DeviceContext* context)
    {
        context->OMSetDepthStencilState(dss_disable_depth_test_, 0);
        dune::gilga_mesh::render(context, nullptr);
        context->OMSetDepthStencilState(dss_enable_depth_test_, 0);
    }

    /*! \brief Set the environment map (latlong format) to use for the sky. */
    void set_envmap(ID3D11Device* device, const dune::tstring& file)
    {
        dune::load_texture(device, file, &envmap_);

        for (auto i = meshes_.begin(); i != meshes_.end(); ++i)
            i->diffuse_tex = envmap_;
    }

    /*! \brief Set an optional cloud map (latlong format) to use for the sky. */
    void set_clouds(ID3D11Device* device, const dune::tstring& file)
    {
        dune::load_texture(device, file, &clouds_);

        for (auto i = meshes_.begin(); i != meshes_.end(); ++i)
            i->specular_tex = clouds_;
    }
};
