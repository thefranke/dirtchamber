/*
 * Dune D3D library - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_LIGHT
#define DUNE_LIGHT

#include <D3D11.h>

#include "cbuffer.h"
#include "gbuffer.h"

namespace dune
{
    /*!
     * \brief A simple directional light source with an RSM.
     *
     * This class is a wrapper for a directional light source rendering to
     * a Reflective Shadow Map (RSM). It manages a constant buffer with
     * information about the light source such as position or view projection
     * matrix for rendering, as well as the gbuffer object to render the
     * RSM to.
     */
    class directional_light
    {
    public:
        struct param
        {
            DirectX::XMFLOAT4   position;
            DirectX::XMFLOAT4   direction;
            DirectX::XMFLOAT4   flux;
            DirectX::XMFLOAT4X4 vp;
            DirectX::XMFLOAT4X4 vp_inv;
            DirectX::XMFLOAT4X4 vp_tex;
        };

    protected:
        gbuffer rsm_;

        typedef cbuffer<param> cb_param;
        cb_param cb_param_;

    public:
        virtual void create(ID3D11Device* device);

        virtual void create(ID3D11Device* device, const tstring& name,
                            const DXGI_SURFACE_DESC& colors, const DXGI_SURFACE_DESC& normals, const DXGI_SURFACE_DESC& depth);

        virtual void create(ID3D11Device* device, const tstring& name,
                            const D3D11_TEXTURE2D_DESC& colors, const D3D11_TEXTURE2D_DESC& normals, const D3D11_TEXTURE2D_DESC& depth);

        virtual void destroy();

        //!@{
        /*! \brief Return local cbuffer parameters. */
        cb_param& parameters() { return cb_param_; }
        const cb_param& parameters() const  { return cb_param_; }
        //!@}

        /*! \brief Update the view-projection matrix in the parameter cbuffer for shadowmap/RSM rendering. */
        void update(const DirectX::XMFLOAT4& upt, const DirectX::XMMATRIX& light_proj);

        gbuffer& rsm();
        void prepare_render(ID3D11DeviceContext* context);
    };

    /*!
     * \brief A differential directional light source.
     *
     * A differential directional light source contains two RSMs \\( \\mu \\) and \\( \\rho \\):
     * - \\( \\rho \\) is an RSM of a scene with an additional object
     * - \\( \\mu \\) is an RSM of the same scene without the additional object
     *
     * Having these two RSMs, the differential indirect bounce can be extracted
     * by subtracing \\( \\mu \\) from \\( \\rho \\). This class is used to intialize delta_light_propagation_volume
     * objects or delta_sparse_voxel_octree objects.
     */
    class differential_directional_light : public directional_light
    {
    protected:
        gbuffer mu_;

    public:
        /*! \brief Return RSM \\( \\rho \\). */
        gbuffer& rho() { return rsm(); }

        /*! \brief Return RSM \\( \\mu \\). */
        gbuffer& mu()  { return mu_;   }

        virtual void create(ID3D11Device* device, const tstring& name,
                            const D3D11_TEXTURE2D_DESC& colors, const D3D11_TEXTURE2D_DESC& normals, const D3D11_TEXTURE2D_DESC& depth);

        virtual void destroy();
    };

    /*
    class point_light : public shader_resource
    {
    };

    class environment_light : public shader_resource
    {
    };
    */
}

#endif
