/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_MATH_TOOLS
#define DUNE_MATH_TOOLS

#include <DirectXMath.h>

namespace dune
{
    const float PI = 3.1415926f;
    const float DEG_TO_RAD = PI/180.f;

    /*! \brief Get a number of the Halton sequence. */
    float halton(int index, int base);
    DirectX::XMMATRIX make_projection(float z_near, float z_far);

    /*! \brief Approximate functions namespace. */
    namespace approx
    {
        inline double sin(double x);
        inline double exp(double x);
    }

    /*! \brief Helper functions to convert PBRT matrix operations. */
    namespace pbrt
    {
        DirectX::XMMATRIX translate(float x, float y, float z);
        DirectX::XMMATRIX rotate(float angle, float x, float y, float z);
    }
}

#endif
