/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "math_tools.h"

#include <cmath>
#include <vector>
#include <algorithm>

namespace dune
{
    float halton(int index, int base)
    {
        float result = 0;
        float f = 1.f / base;
        int i = index;

        while (i > 0)
        {
            result = result + f * static_cast<float>(i % base);
            i = static_cast<int>(std::floor(static_cast<float>(i)/ base));
            f = f / base;
        }

        return result;
    }

    void convert_handness(DirectX::XMMATRIX& m)
    {
        DirectX::XMFLOAT4X4 mat;
        DirectX::XMStoreFloat4x4(&mat, m);

        for (size_t i = 0; i < 4; ++i)
            std::swap(mat.m[2][i], mat.m[1][i]);

        for (size_t i = 0; i < 4; ++i)
            std::swap(mat.m[i][2], mat.m[i][1]);

        m = DirectX::XMLoadFloat4x4(&mat);
    }

    DirectX::XMMATRIX make_projection(float z_near, float z_far)
    {
        float n = z_near;
        float f = z_far;

        float q = f/(f-n);

        return DirectX::XMMATRIX
        (
            1.f, 0.f,  0.f, 0.f,
            0.f, 1.f,  0.f, 0.f,
            0.f, 0.f,    q, 1.f,
            0.f, 0.f, -q*n, 0.f
        );
    }

    namespace approx
    {
        inline double sin(double x)
        {
            return x - x * x * x * (0.1587164 + x * x * 0.00585375);
        }

        inline double exp(double x)
        {
            const double a0 = 1.000090756764725753887362987792025308996;
            const double a1 = 9.973086551667860566788019540269306006270e-1;
            const double a2 = 4.988332174505582284710918757571761729419e-1;
            const double a3 = 1.773462612793916519454714108029230813767e-1;
            const double a4 = 4.415666059995979611944324860870682575219e-2;

            return a0 + x * (a1 + x * (a2 + x * (a3 + x * a4)));
        }

        // http://www.johndcook.com/blog/2012/07/25/trick-for-computing-log1x/
        template<typename T> static inline T log1p(T x) throw()
        {
            volatile T y = 1 + x, z = y - 1;
            return z == 0 ? x : x * std::log(y) / z;
        }
    }

    namespace pbrt
    {
        DirectX::XMMATRIX translate(float x, float y, float z)
        {
            return DirectX::XMMatrixTranslation(x, -y/2.f, z);
        }

        DirectX::XMMATRIX rotate(float angle, float x, float y, float z)
        {
            DirectX::XMMATRIX rot;
            DirectX::XMVECTORF32 axis = { x, y, z, 0.f };

            return DirectX::XMMatrixRotationAxis(axis, angle);
        }
    }
}
