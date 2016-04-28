/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "composite_mesh.h"

#include <algorithm>

#include "d3d_tools.h"
#include "unicode.h"
#include "exception.h"
#include "common_tools.h"

namespace dune
{
    void composite_mesh::set_world(const DirectX::XMFLOAT4X4& world)
    {
        world_ = world;
        std::for_each(meshes_.begin(), meshes_.end(), [&](mesh_ptr& m){ m->set_world(world); });
    }

    void composite_mesh::set_shader(ID3D11Device* device, ID3DBlob* input_binary, ID3D11VertexShader* vs, ID3D11PixelShader* ps)
    {
        exchange(&vs_, vs);
        exchange(&ps_, ps);

        std::for_each(meshes_.begin(), meshes_.end(), [&](mesh_ptr& m){ m->set_shader(device, input_binary, vs, ps); });
    }

    void composite_mesh::create(ID3D11Device* device, const tstring& file)
    {
        mesh_ptr m;

        vs_ = nullptr;
        ps_ = nullptr;

        load_model(device, file, m);

        if (meshes_.empty())
            init_bb(m->bb_min());
        else
            update_bb(m->bb_min());

        update_bb(m->bb_max());

        meshes_.push_back(m);
    }

    void composite_mesh::create_from_dir(ID3D11Device* device, const tstring& pattern)
    {
        tstring absolute_pattern = make_absolute_path(pattern);
        tstring path = extract_path(tstring(absolute_pattern));

        std::vector<WIN32_FIND_DATA> files;

        WIN32_FIND_DATA data;

        HANDLE h = FindFirstFile(absolute_pattern.c_str(), &data);

        if (h != INVALID_HANDLE_VALUE)
        {
            files.push_back(data);

            while(FindNextFile(h, &data))
                files.push_back(data);
        }

        if (files.empty())
            throw exception(tstring(L"Couldn't find ") + pattern);

        for (auto i = files.begin(); i != files.end(); ++i)
        {
            tstring file = path + tstring(i->cFileName);
            create(device, file.c_str());
        }
    }

    void composite_mesh::render(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* to_clip)
    {
        std::for_each(meshes_.begin(), meshes_.end(), [&](mesh_ptr& m){ m->render(context, to_clip); });
    }

    void composite_mesh::destroy()
    {
        d3d_mesh::destroy();

        std::for_each(meshes_.begin(), meshes_.end(), [&](mesh_ptr& m){ m->destroy(); });
        meshes_.clear();
    }

    size_t composite_mesh::num_vertices()
    {
        size_t n = 0;
        std::for_each(meshes_.begin(), meshes_.end(), [&n](mesh_ptr& m){ n += m->num_vertices(); });
        return n;
    }

    size_t composite_mesh::num_faces()
    {
        size_t n = 0;
        std::for_each(meshes_.begin(), meshes_.end(), [&n](mesh_ptr& m){ n += m->num_faces(); });
        return n;
    }

    void composite_mesh::push_back(mesh_ptr& m)
    {
        meshes_.push_back(m);
    }

    void composite_mesh::set_shader_slots(INT diffuse_tex, INT specular_tex, INT normal_tex)
    {
        std::for_each(meshes_.begin(), meshes_.end(), [&](mesh_ptr& m){ m->set_shader_slots(diffuse_tex, specular_tex, normal_tex); });
    }

    composite_mesh::mesh_ptr composite_mesh::operator[](size_t i)
    {
        return meshes_[i];
    }
}
