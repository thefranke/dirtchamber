/* 
 * dune::sumed_area_table by Tobias Alexander Franke (tob@cyberhead.de) 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "summed_area_tables.h"

namespace dune 
{
    inline float summed_area_table::sum(int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy) const
    {
        return I(cx, cy) + I(ax, ay) - I(bx, by) - I(dx, dy);
    }

    float summed_area_table::I(int x, int y) const
    {
        if (x < 0 || y < 0) return 0;
        size_t i = y*width_ + x;
        return sat_[i];
    }

    void sat_region::create(int x, int y, int w, int h, const summed_area_table* sat, float init_sum)
    {
	    x_ = x; y_ = y; w_ = w; h_ = h; sum_ = init_sum; sat_ = sat;

	    if (sum_ < 0)
		    sum_ = sat_->sum(x,       y, 
						     x+(w-1), y, 
						     x+(w-1), y+(h-1),
						     x,       y+(h-1));
    }

    void sat_region::split_w(sat_region& A) const
    {
        for (int w = 1; w <= w_; ++w)
        {
            A.create(x_, y_, w, h_, sat_);

            // if region left has approximately half the energy of the entire thing stahp
            if (A.sum_*2.f >= sum_)
                break;
        }
    }

    void sat_region::split_w(sat_region& A, sat_region& B) const
    {
        split_w(A);
        B.create(x_ + (A.w_-1), y_, w_ - A.w_, h_, sat_, sum_ - A.sum_);
    }

    void sat_region::split_h(sat_region& A) const
    {
        for (int h = 1; h <= h_; ++h)
        {
            A.create(x_, y_, w_, h, sat_);

            // if region top has approximately half the energy of the entire thing stahp
            if (A.sum_*2.f >= sum_)
                break;
        }
    }

    void sat_region::split_h(sat_region& A, sat_region& B) const
    {
        split_h(A);
        B.create(x_, y_ + (A.h_-1), w_, h_ - A.h_, sat_, sum_ - A.sum_);
    }

    DirectX::XMFLOAT2 sat_region::centroid() const
    {
        DirectX::XMFLOAT2 c;

        sat_region A;
            
        split_w(A);
        c.x = static_cast<FLOAT>(A.x_ + (A.w_-1));

        split_h(A);
        c.y = static_cast<FLOAT>(A.y_ + (A.h_-1));
            
        return c;
    }

    void split(const sat_region& r, size_t n, std::vector<sat_region>& regions)
    {
        // check: can't split any further?
        if (r.w_ < 2 || r.h_ < 2 || n == 0)
        {
            regions.push_back(r);
            return;
        }

        sat_region A, B;

        if (r.w_ > r.h_)
            r.split_w(A, B);
        else
            r.split_h(A, B);

        split(A, n-1, regions);
        split(B, n-1, regions);
    }

    void median_cut(const BYTE* rgba, size_t width, size_t height, size_t n, std::vector<sat_region>& regions, summed_area_table& img)
    {
        img.create_lum(rgba, width, height, 4);

        regions.clear();

        // insert entire image as start region
        sat_region r;
        r.create(0, 0, static_cast<int>(width), static_cast<int>(height), &img);

        // recursively split into subregions
        split(r, n, regions);
    }

    void median_cut(const BYTE* rgba, size_t width, size_t height, size_t n, std::vector<environment_light>& lights)
    {
        std::vector<sat_region> regions;
        regions.clear();

        summed_area_table sat;
        median_cut(rgba, width, height, n, regions, sat);

        lights.clear();

        // set light at center/centroid of region with energy of region
        for (size_t i = 0; i < regions.size(); ++i)
        {
            environment_light light;

            // TODO: make latlong out of pos
            light.pos = regions[i].centroid();

            light.theta = static_cast<float>(2.0 * std::acos(std::sqrt(1.0 - light.pos.y)));
            light.phi   = static_cast<float>(light.pos.x * DirectX::XM_PI);
        
            if (light.pos.y < 0 || light.pos.x < 0)
                continue;

            size_t ci = static_cast<size_t>(light.pos.y * width + light.pos.x)*4;
            light.flux.x = rgba[ci + 0] * regions[i].sum_;
            light.flux.y = rgba[ci + 1] * regions[i].sum_;
            light.flux.z = rgba[ci + 2] * regions[i].sum_;

            lights.push_back(light);
        }
    }
}

