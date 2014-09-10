/*
 * dune::camera by Tobias Alexander Franke (tob@cyberhead.de) 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_CAMERA
#define DUNE_CAMERA

#undef NOMINMAX
#include <DXUT.h>
#include <DXUTcamera.h>

#include "cbuffer.h"

namespace dune
{
    /*!
     * \brief A simple perspective camera.
     *
     * This class derives from CFirstPersonCamera in DXUT and simply handles
     * a constant buffer for the camera parameters.
     */
    class camera : public CFirstPersonCamera
    {
    public:
        struct param
        {
            DirectX::XMFLOAT4X4 vp;
            DirectX::XMFLOAT4X4 vp_inv;
            DirectX::XMFLOAT3 camera_pos;
            FLOAT z_far;
        };

    protected:
        typedef cbuffer<param> cb_param;
        cb_param cb_param_;

    public:
        void create(ID3D11Device* device);
        void destroy();

        //!@{
        /*! \brief Return local cbuffer parameters. */
        cb_param& parameters() { return cb_param_; }
        const cb_param& parameters() const { return cb_param_; }
        //!@}

        /*! \brief Update the constant buffer from the current values of the camera and a supplied Z-far. */
        void update(float z_far);
    };
}

#endif
