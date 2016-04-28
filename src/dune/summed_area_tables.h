/*
 * Dune D3D library - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_SUMMED_AREA_TABLES
#define DUNE_SUMMED_AREA_TABLES

#include <cassert>
#include <vector>

#include <D3D11.h>
#include <DirectXMath.h>

namespace dune
{
    template<typename T>
    T luminance(const T& r, const T& g, const T& b)
    {
        T v = static_cast<T>(0.2126*r + 0.7152*g + 0.0722*b);
        return v*v;
    }

    struct environment_light
    {
        float theta, phi;
        DirectX::XMFLOAT2 pos;
        DirectX::XMFLOAT3 flux;
    };

    class summed_area_table
    {
    protected:
        int width_, height_;
        std::vector<float> sat_;

        float I(int x, int y) const;

    public:
        //template<typename T>
        void create_lum(const BYTE* rgb, size_t width, size_t height, int nc)
        {
            assert(nc > 2);

            width_ = static_cast<int>(width); height_ = static_cast<int>(height);

            sat_.clear();
            sat_.resize(width_ * height_);

            for (int y = 0; y < height_; ++y)
            for (int x = 0; x < width_;  ++x)
            {
                size_t i = y*width_ + x;

                float r = static_cast<float>(rgb[i*nc + 0]);
                float g = static_cast<float>(rgb[i*nc + 1]);
                float b = static_cast<float>(rgb[i*nc + 2]);

                float ixy = luminance(r,g,b);

                sat_[i] = ixy + I(x-1, y) + I(x, y-1) - I(x-1, y-1);
            }
        }

        int width() const  { return width_;  }
        int height() const { return height_; }

        /*!
         * \brief Returns the sum of a region defined by A,B,C,D.
         *
         * A----B
         * |    |  sum = C+A-B-D
         * D----C
         *
         */
        float sum(int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy) const;
    };

    struct sat_region
    {
        int x_, y_, w_, h_;
        float sum_;
        const summed_area_table* sat_;

        void create(int x, int y, int w, int h, const summed_area_table* sat, float init_sum = -1);

        /*! \brief Split region horizontally into subregions A and B. */
        void split_w(sat_region& A, sat_region& B) const;
        void split_w(sat_region& A) const;

        /*! \brief Split region vertically into subregions A and B. */
        void split_h(sat_region& A, sat_region& B) const;
        void split_h(sat_region& A) const;

        DirectX::XMFLOAT2 centroid() const;
    };

    void median_cut(const BYTE* rgba, size_t width, size_t height, size_t n, std::vector<sat_region>& regions, summed_area_table& img);
    void median_cut(const BYTE* rgba, size_t width, size_t height, size_t n, std::vector<environment_light>& lights);
}

#endif
