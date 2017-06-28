/*
 * Dune D3D library - Tobias Alexander Franke 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "serializer_tools.h"

#include "camera.h"
#include "light.h"
#include "light_propagation_volume.h"
#include "sparse_voxel_octree.h"

namespace dune
{
    serializer& operator<<(serializer& s, const camera& c)
    {
        auto eye = c.GetEyePt();
        {
            s.put(L"camera.pos.x", DirectX::XMVectorGetX(eye));
            s.put(L"camera.pos.y", DirectX::XMVectorGetY(eye));
            s.put(L"camera.pos.z", DirectX::XMVectorGetZ(eye));
        }

        auto V = c.GetLookAtPt();
        {
            s.put(L"camera.lookat.x", DirectX::XMVectorGetX(V));
            s.put(L"camera.lookat.y", DirectX::XMVectorGetY(V));
            s.put(L"camera.lookat.z", DirectX::XMVectorGetZ(V));
        }

        return s;
    }

    const serializer& operator>>(const serializer& s, camera& c)
    {
        try
        {
            float px = s.get<float>(L"camera.pos.x");
            float py = s.get<float>(L"camera.pos.y");
            float pz = s.get<float>(L"camera.pos.z");

            DirectX::XMVECTOR eye = { px, py, pz, 1.f };

            float vx = s.get<float>(L"camera.lookat.x");
            float vy = s.get<float>(L"camera.lookat.y");
            float vz = s.get<float>(L"camera.lookat.z");

            DirectX::XMVECTOR V = { vx, vy, vz, 1.f };

            c.SetViewParams(eye, V);
        }
        catch (dune::exception& e)
        {
            tcerr << L"Couldn't load camera settings: " << e.msg() << std::endl;
        }

        return s;
    }

    serializer& operator<<(serializer& s, const directional_light& l)
    {
        auto& p = l.parameters().data();
        {
            s.put(L"light.position.x",  p.position.x);
            s.put(L"light.position.y",  p.position.y);
            s.put(L"light.position.z",  p.position.z);

            s.put(L"light.direction.x", p.direction.x);
            s.put(L"light.direction.y", p.direction.y);
            s.put(L"light.direction.z", p.direction.z);

            s.put(L"light.flux.r",      p.flux.x);
            s.put(L"light.flux.g",      p.flux.y);
            s.put(L"light.flux.b",      p.flux.z);
        }

        return s;
    }

    const serializer& operator>>(const serializer& s, directional_light& l)
    {
        try
        {
            auto& p = l.parameters().data();
            {
                p.position.x   = s.get<float>(L"light.position.x");
                p.position.y   = s.get<float>(L"light.position.y");
                p.position.z   = s.get<float>(L"light.position.z");
                p.position.w   = 1.f;

                p.direction.x  = s.get<float>(L"light.direction.x");
                p.direction.y  = s.get<float>(L"light.direction.y");
                p.direction.z  = s.get<float>(L"light.direction.z");
                p.direction.w  = 0.f;

                p.flux.x       = s.get<float>(L"light.flux.r");
                p.flux.y       = s.get<float>(L"light.flux.g");
                p.flux.z       = s.get<float>(L"light.flux.b");
                p.flux.w       = 0.f;
            }
        }
        catch (dune::exception& e)
        {
            tcerr << L"Couldn't load directional light parameters: " << e.msg() << std::endl;
        }

        return s;
    }

    serializer& operator<<(serializer& s, const light_propagation_volume& lpv)
    {
        auto& p = lpv.parameters().data();
        {
            s.put(L"gi.scale",                  p.vpl_scale);
            s.put(L"gi.debug",                  p.debug_gi);
            s.put(L"gi.lpv.flux_amplifier",     p.lpv_flux_amplifier);
            s.put(L"gi.lpv.num_propagations",   lpv.num_propagations());
        }

        return s;
    }

    const serializer& operator>>(const serializer& s, light_propagation_volume& lpv)
    {
        try
        {
            auto& p = lpv.parameters().data();
            {
                p.vpl_scale             = s.get<float>(L"gi.scale");
                p.debug_gi              = s.get<bool>(L"gi.debug");
                p.lpv_flux_amplifier    = s.get<float>(L"gi.lpv.flux_amplifier");
            }

            lpv.set_num_propagations(s.get<size_t>(L"gi.lpv.num_propagations"));
        }
        catch (dune::exception& e)
        {
            tcerr << "Couldn't load LPV parameters: " << e.msg() << std::endl;
        }

        return s;
    }

    serializer& operator<<(serializer& s, const sparse_voxel_octree& svo)
    {
        auto& p = svo.parameters().data();
        {
            s.put(L"gi.scale", p.gi_scale);
            s.put(L"gi.debug", p.debug_gi);
            s.put(L"gi.svo.glossiness", p.glossiness);
        }

        return s;
    }

    const serializer& operator>>(const serializer& s, sparse_voxel_octree& svo)
    {
        try
        {
            auto& p = svo.parameters().data();
            {
                p.gi_scale = s.get<float>(L"gi.scale");
                p.debug_gi = s.get<bool>(L"gi.debug");
                p.glossiness = s.get<float>(L"gi.svo.glossiness");
            }
        }
        catch (dune::exception& e)
        {
            tcerr << "Couldn't load LPV parameters: " << e.msg() << std::endl;
        }

        return s;
    }
}
