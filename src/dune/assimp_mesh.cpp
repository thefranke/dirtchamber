/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "assimp_mesh.h"

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iostream>

#include <CommCtrl.h>
#include <assimp/ProgressHandler.hpp>

#include "d3d_tools.h"
#include "texture_cache.h"
#include "common_tools.h"
#include "exception.h"

namespace dune
{
    namespace detail
    {
        DirectX::XMFLOAT3 aivec_to_dxvec3(aiVector3D v)
        {
            return DirectX::XMFLOAT3(v.x, v.y, v.z);
        }

        DirectX::XMFLOAT4 aivec_to_dxvec4(aiColor4D v)
        {
            return DirectX::XMFLOAT4(v.r, v.g, v.b, v.a);
        }

        DirectX::XMFLOAT2 aivec_to_dxvec2(aiVector3D v)
        {
            return DirectX::XMFLOAT2(v.x, v.y);
        }

        tstring to_tstring(aiString s)
        {
            std::string xs(s.data, s.length);
            return dune::to_tstring(xs);
        }
    }

    assimp_mesh::assimp_mesh() :
        importer_(),
        mesh_infos_(),
        indices_(),
        num_faces_(0),
        num_vertices_(0)
    {
    }

    const aiScene* const assimp_mesh::assimp_scene() const
    {
        return importer_.GetScene();
    }

    void assimp_mesh::destroy()
    {
        d3d_mesh::destroy();
        mesh_infos_.clear();
        indices_.clear();
        num_faces_ = 0;
        num_vertices_ = 0;
    }

    size_t assimp_mesh::num_vertices()
    {
        size_t n = 0;

        for (size_t m = 0; m < assimp_scene()->mNumMeshes; ++m)
            n += assimp_scene()->mMeshes[m]->mNumVertices;

        return n;
    }

    size_t assimp_mesh::num_faces()
    {
        size_t n = 0;

        for (size_t m = 0; m < assimp_scene()->mNumMeshes; ++m)
            n += assimp_scene()->mMeshes[m]->mNumFaces;

        return n;
    }

    struct progress_handler : public Assimp::ProgressHandler
    {
        virtual bool Update(float percentage)
        {
            return true;
        }
    };

    void assimp_mesh::load(const tstring& file)
    {
        std::string name = to_string(make_absolute_path(file));
        tclog << L"Loading: " << file << std::endl;

        progress_handler ph;
        importer_.SetProgressHandler(&ph);

        const aiScene* scene = importer_.ReadFile(name.c_str(),
            aiProcess_CalcTangentSpace |
            aiProcess_Triangulate |
            aiProcess_MakeLeftHanded |
            aiProcess_JoinIdenticalVertices |
            aiProcess_SortByPType |
            aiProcess_GenSmoothNormals |
            //aiProcess_GenNormals |
            aiProcess_RemoveRedundantMaterials |
            aiProcess_OptimizeMeshes |
            aiProcess_GenUVCoords |
            aiProcess_TransformUVCoords);

        if (!scene)
        {
            std::string error = importer_.GetErrorString();

            tstringstream ss;
            ss << L"Failed loading: " << file << std::endl;
            ss << error.c_str() << std::endl;
            throw exception(ss.str());
        }

        aiMatrix4x4 m;

        load_internal(scene, scene->mRootNode, m);

        importer_.SetProgressHandler(nullptr);

        tclog << L"Mesh info: " << std::endl
              << L" - " << mesh_infos_.size() << L" meshes" << std::endl
              << L" - " << "BBOX ("
                        << bb_max().x << L", "
                        << bb_max().y << L", "
                        << bb_max().z << L")"
                        << L" - ("
                        << bb_min().x << L", "
                        << bb_min().y << L", "
                        << bb_min().z << L")"
                        << std::endl
              << L" - " << num_faces_ << L" faces" << std::endl
              << L" - " << num_vertices_ << L" vertices" << std::endl;
    }

    void assimp_mesh::load_internal(const aiScene* scene, aiNode* node, aiMatrix4x4 acc_transform)
    {
        aiMatrix4x4 transform = acc_transform * node->mTransformation;

        // store all meshes, one after another
        for(size_t a = 0; a < node->mNumMeshes; ++a)
        {
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[a]];

            // store current read num_vertices_ and aiMesh for potential reference
            mesh_info m;
            m.vstart_index   = num_vertices_;
            m.istart_index   = static_cast<UINT>(indices_.size());
            m.num_faces      = mesh->mNumFaces;
            m.num_vertices   = mesh->mNumVertices;
            m.material_index = mesh->mMaterialIndex;

            num_vertices_ += mesh->mNumVertices;
            num_faces_    += mesh->mNumFaces;

            // store vertices
            for(size_t b = 0; b < mesh->mNumVertices; ++b)
            {
                assimp_mesh::vertex v;

                aiVector3D v_trans = transform * mesh->mVertices[b];

                v.position = detail::aivec_to_dxvec3(v_trans);

                // if this is the very first vertex
                if (m.vstart_index == 0 && b == 0)
                    init_bb(v.position);
                else
                    update_bb(v.position);

                if (mesh->HasNormals())
                    v.normal = detail::aivec_to_dxvec3(mesh->mNormals[b]);

                if (mesh->HasTangentsAndBitangents())
                    v.tangent = detail::aivec_to_dxvec3(mesh->mTangents[b]);

                for (size_t n = 0; n < mesh->GetNumUVChannels(); ++n)
                {
                    v.texcoord[n] = detail::aivec_to_dxvec2(mesh->mTextureCoords[n][b]);
                    v.texcoord[n].y = 1.f - v.texcoord[n].y;
                }

                push_back(v);

                m.update(v.position, b == 0);
            }

            // store indices, corrected by startIndex, and attribute
            for(size_t b = 0; b < mesh->mNumFaces; ++b)
            {
                indices_.push_back(mesh->mFaces[b].mIndices[2]);
                indices_.push_back(mesh->mFaces[b].mIndices[1]);
                indices_.push_back(mesh->mFaces[b].mIndices[0]);
            }

            mesh_infos_.push_back(m);
        }

        for (size_t c = 0; c < node->mNumChildren; ++c)
            assimp_mesh::load_internal(scene, node->mChildren[c], transform);
    }

    void gilga_mesh::push_back(vertex v)
    {
        gilga_vertex gv;

        gv.normal = v.normal;
        gv.position = v.position;
        gv.texcoord = v.texcoord[0];
        gv.tangent = v.tangent;

        vertices_.push_back(gv);
    }

    void gilga_mesh::prepare_context(ID3D11DeviceContext* context)
    {
        context->VSSetShader(vs_, nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(ps_, nullptr, 0);

        ss_.to_ps(context, 0);

        context->OMSetBlendState(nullptr, nullptr, 0xFF);
    }

    void gilga_mesh::render(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* to_clip)
    {
        prepare_context(context);
        render_direct(context, to_clip);
    }

    void gilga_mesh::render_direct(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* to_clip)
    {
        assert(context);
        context->IASetInputLayout(vertex_layout_);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        auto cbvs = &cb_mesh_data_vs_.data();
        {
            DirectX::XMStoreFloat4x4(&cbvs->world,
                DirectX::XMLoadFloat4x4(&world()));
        }
        cb_mesh_data_vs_.to_vs(context, 1);

        mesh_data* prev = nullptr;

        for (size_t m = 0; m < meshes_.size(); ++m)
        {
            mesh_data* data = &meshes_[m];
            mesh_info* info = &mesh_infos_[m];

            if (to_clip && !is_visible(*info, *to_clip))
                continue;

            // check if material has changed
            bool same = (prev &&
                equal(prev->diffuse_color, data->diffuse_color) &&
                equal(prev->specular_color, data->specular_color) &&
                equal(prev->emissive_color, data->emissive_color) &&
                prev->diffuse_tex == data->diffuse_tex &&
                prev->specular_tex == data->specular_tex &&
                prev->normal_tex == data->normal_tex &&
                prev->specular_tex == data->specular_tex &&
                prev->alpha_tex == data->alpha_tex &&
                prev->shading_mode == data->shading_mode &&
                prev->roughness == data->roughness
                );

            if (!same)
            {
                const bool has_diffuse_tex = data->diffuse_tex != nullptr && diffuse_tex_slot_ != -1;
                const bool has_normal_tex = data->normal_tex != nullptr && normal_tex_slot_ != -1;
                const bool has_specular_tex = data->specular_tex != nullptr && specular_tex_slot_ != -1;
                const bool has_alpha_tex = data->alpha_tex != nullptr && alpha_tex_slot_ != -1;

                auto cbps = &cb_mesh_data_ps_.data();
                {
                    cbps->diffuse_color = data->diffuse_color;
                    cbps->specular_color = data->specular_color;
                    cbps->emissive_color = data->emissive_color;
                    cbps->has_diffuse_tex = has_diffuse_tex;
                    cbps->has_normal_tex = has_normal_tex;
                    cbps->has_specular_tex = has_specular_tex;
                    cbps->has_alpha_tex = has_alpha_tex;
                    cbps->shading_mode = data->shading_mode;
                    cbps->roughness = data->roughness;
                    cbps->refractive_index = data->refractive_index;
                }
                cb_mesh_data_ps_.to_ps(context, 0);

                if (has_diffuse_tex)
                    context->PSSetShaderResources(diffuse_tex_slot_, 1, &data->diffuse_tex);

                if (has_normal_tex)
                    context->PSSetShaderResources(normal_tex_slot_, 1, &data->normal_tex);

                if (has_specular_tex)
                    context->PSSetShaderResources(specular_tex_slot_, 1, &data->specular_tex);

                if (has_alpha_tex)
                    context->PSSetShaderResources(alpha_tex_slot_, 1, &data->alpha_tex);
            }

            context->IASetIndexBuffer(data->index, DXGI_FORMAT_R32_UINT, 0);

            static const UINT stride = sizeof(gilga_vertex);
            static const UINT offset = 0;
            context->IASetVertexBuffers(0, 1, &data->vertex, &stride, &offset);

            context->DrawIndexed(info->num_faces * 3, 0, 0);

            prev = &meshes_[m];
        }
    }

    gilga_mesh::gilga_mesh() :
        cb_mesh_data_ps_(),
        cb_mesh_data_vs_(),
        ss_(),
        alpha_tex_slot_(-1),
        vertices_(),
        meshes_()
    {
    }

    void gilga_mesh::create(ID3D11Device* device, const tstring& file)
    {
        DirectX::XMStoreFloat4x4(&world_, DirectX::XMMatrixIdentity());

        load(file);

        cb_mesh_data_vs_.create(device);
        cb_mesh_data_ps_.create(device);

        D3D11_SAMPLER_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;

        ss_.create(device, sd);

        tstring path = extract_path(file);

        for (auto i = mesh_infos_.begin(); i != mesh_infos_.end(); ++i)
        {
            UINT num_vertices = i->num_vertices;
            UINT num_faces = i->num_faces;

            if (num_faces == 0 || num_vertices == 0)
            {
                tcout << L"Skipping invalid mesh with " <<
                    num_vertices << L" vertices and " <<
                    num_faces << L" faces" << std::endl;
                continue;
            }

            mesh_data mesh;
            ZeroMemory(&mesh, sizeof(mesh));

            D3D11_BUFFER_DESC bd;
            D3D11_SUBRESOURCE_DATA initdata;

            // create vertex buffer
            ZeroMemory(&bd, sizeof(bd));
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(gilga_vertex) * num_vertices;
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            ZeroMemory(&initdata, sizeof(initdata));
            initdata.pSysMem = &vertices_[i->vstart_index];

            assert_hr(device->CreateBuffer(&bd, &initdata, &mesh.vertex));

            // create index buffer
            ZeroMemory(&bd, sizeof(bd));
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(UINT) * (num_faces * 3);
            bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
            bd.CPUAccessFlags = 0;

            ZeroMemory(&initdata, sizeof(initdata));
            initdata.pSysMem = &indices_[i->istart_index];

            assert_hr(device->CreateBuffer(&bd, &initdata, &mesh.index));

            // load material
            aiMaterial* mat = assimp_scene()->mMaterials[i->material_index];

            aiColor4D color;

            // grab diffuse color
            mesh.diffuse_color = DirectX::XMFLOAT4(0.f, 0.f, 0.f, 0.f);
            if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
                mesh.diffuse_color = detail::aivec_to_dxvec4(color);

            // grab opacity and insert into diffuse color.a
            float opacity = 0;
            if (mat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS)
                mesh.diffuse_color.w = opacity;

            // grab specular color
            mesh.specular_color = DirectX::XMFLOAT4(0.f, 0.f, 0.f, 0.f);
            if (mat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
                mesh.specular_color = detail::aivec_to_dxvec4(color);

            // grab emissive color
            mesh.emissive_color = DirectX::XMFLOAT4(0.f, 0.f, 0.f, 0.f);
            if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
                mesh.emissive_color = detail::aivec_to_dxvec4(color);

            // get the shading mode -> abuse as basic material shading map
            int shading_mode;

            mesh.shading_mode = SHADING_DEFAULT;
            if (mat->Get(AI_MATKEY_SHADING_MODEL, shading_mode) == AI_SUCCESS)
            {
                // convert back, don't care about the specific expected implementation
                switch (shading_mode)
                {
                case 9:  mesh.shading_mode = SHADING_OFF;       break;
                case 3:  mesh.shading_mode = SHADING_DEFAULT;   break;
                case 2:  mesh.shading_mode = SHADING_IBL;       break;

                default: mesh.shading_mode = SHADING_DEFAULT;   break;
                };

                if (mesh.emissive_color.x + mesh.emissive_color.y + mesh.emissive_color.z > 0)
                    mesh.shading_mode = SHADING_AREA_LIGHT;
            }

            // get specular exponent
            // WARNING: OBJ specifies shininess = Ns * 4
            // convert shininess to roughness first
            float shininess;

            mesh.roughness = 0.0f;
            if (mat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
                mesh.roughness = std::sqrtf(std::sqrtf(2.0f / (shininess + 2.0f)));

            // get refractive index
            float refractive_index;

            mesh.refractive_index = 1.5;
            if (mat->Get(AI_MATKEY_REFRACTI, refractive_index) == AI_SUCCESS)
                mesh.refractive_index = refractive_index;

            // get textures
            aiString str;

            str.Clear();
            if (mat->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), str) == AI_SUCCESS)
                load_texture(device, path + detail::to_tstring(str), &mesh.diffuse_tex);

            str.Clear();
            if (mat->Get(AI_MATKEY_TEXTURE_EMISSIVE(0), str) == AI_SUCCESS)
                load_texture(device, path + detail::to_tstring(str), &mesh.emissive_tex);

            str.Clear();
            if (mat->Get(AI_MATKEY_TEXTURE_SPECULAR(0), str) == AI_SUCCESS)
                load_texture(device, path + detail::to_tstring(str), &mesh.specular_tex);

            str.Clear();
            //mat->Get(AI_MATKEY_TEXTURE_NORMALS(0), str);
            if (mat->Get(AI_MATKEY_TEXTURE_HEIGHT(0), str) == AI_SUCCESS)
                load_texture(device, path + detail::to_tstring(str), &mesh.normal_tex);

            str.Clear();
            if (mat->Get(AI_MATKEY_TEXTURE_OPACITY(0), str) == AI_SUCCESS)
                load_texture(device, path + detail::to_tstring(str), &mesh.alpha_tex);

            meshes_.push_back(mesh);
        }

        importer_.FreeScene();
    }

    void gilga_mesh::destroy()
    {
        assimp_mesh::destroy();

        for (auto i = meshes_.begin(); i != meshes_.end(); ++i)
        {
            safe_release(i->index);
            safe_release(i->vertex);
        }

        ss_.destroy();

        cb_mesh_data_vs_.destroy();
        cb_mesh_data_ps_.destroy();

        vertices_.clear();
        meshes_.clear();

        alpha_tex_slot_ = -1;
    }
}
