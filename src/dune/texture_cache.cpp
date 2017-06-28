/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "texture_cache.h"

#include <iostream>
#include <algorithm>

#include "d3d_tools.h"

#pragma warning(disable: 4996)
#include "../ext/stb/stb_image.h"
#include "../ext/dds/DDSTextureLoader.h"

#include "texture.h"
#include "unicode.h"
#include "exception.h"
#include "common_tools.h"

namespace dune
{
    namespace detail
    {
        void stb_to_srv(ID3D11Device* device, LPCSTR filename, ID3D11ShaderResourceView** srv)
        {
            if (!srv) throw exception(L"No SRV");

            *srv = nullptr;

            bool is_hdr = std::string(filename).find(".hdr") != std::string::npos;
            bool is_jpg = std::string(filename).find(".jpg") != std::string::npos;

            D3D11_TEXTURE2D_DESC tex_desc;
            ZeroMemory(&tex_desc, sizeof(tex_desc));

            D3D11_SUBRESOURCE_DATA subdata;
            ZeroMemory(&subdata, sizeof(subdata));

            int width, height, nc;

            unsigned char* ldr_data = nullptr;
            float* hdr_data = nullptr;

            if (!is_hdr)
            {
                ldr_data = stbi_load(filename, &width, &height, &nc, 4);
                subdata.SysMemPitch = width * 4;
                subdata.pSysMem = ldr_data;

                if (is_jpg)
                    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                else
                    // TODO: Remap to SRGB later when throwing out D3DX
                    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

                // TODO: Need to create mipmaps
            }
            else
            {
                hdr_data = stbi_loadf(filename, &width, &height, &nc, 4);
                subdata.SysMemPitch = width * 4 * 4;
                subdata.pSysMem = hdr_data;
                tex_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            }

            if (!ldr_data && !hdr_data)
            {
                tstring error = L"can't load";

                if (stbi_failure_reason())
                    error = to_tstring(std::string(stbi_failure_reason()));

                throw exception(tstring(L"stbi: ") + error);
            }

            tex_desc.Width = width;
            tex_desc.Height = height;
            tex_desc.MipLevels = 1;
            tex_desc.ArraySize = 1;
            tex_desc.SampleDesc.Count = 1;
            tex_desc.SampleDesc.Quality = 0;
            tex_desc.Usage = D3D11_USAGE_DEFAULT;
            tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            tex_desc.CPUAccessFlags = 0;
            tex_desc.MiscFlags = 0;

            ID3D11Texture2D* texture;

            assert_hr(device->CreateTexture2D(&tex_desc, &subdata, &texture));
            assert_hr(device->CreateShaderResourceView(texture, nullptr, srv));

            safe_release(texture);

            if (is_hdr)
                stbi_image_free(hdr_data);
            else
                stbi_image_free(ldr_data);
        }

        void stb_to_texture(ID3D11Device* device, LPCSTR filename, texture& t)
        {
            bool is_hdr = std::string(filename).find(".hdr") != std::string::npos;
            bool is_jpg = std::string(filename).find(".jpg") != std::string::npos;

            D3D11_TEXTURE2D_DESC tex_desc;
            ZeroMemory(&tex_desc, sizeof(tex_desc));

            D3D11_SUBRESOURCE_DATA subdata;
            ZeroMemory(&subdata, sizeof(subdata));

            int width, height, nc;

            unsigned char* ldr_data = nullptr;
            float* hdr_data = nullptr;

            if (!is_hdr)
            {
                ldr_data = stbi_load(filename, &width, &height, &nc, 4);
                subdata.SysMemPitch = width * 4;
                subdata.pSysMem = ldr_data;

                if (is_jpg)
                    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                else
                    // TODO: Remap to SRGB later when throwing out D3DX
                    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

                // TODO: Need to create mipmaps
            }
            else
            {
                hdr_data = stbi_loadf(filename, &width, &height, &nc, 4);
                subdata.SysMemPitch = width * 4 * 4;
                subdata.pSysMem = hdr_data;
                tex_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            }

            if (!ldr_data && !hdr_data)
            {
                tstring error = L"can't load";

                if (stbi_failure_reason())
                    error = to_tstring(std::string(stbi_failure_reason()));

                throw exception(tstring(L"stbi: ") + error);
            }

            tex_desc.Width = width;
            tex_desc.Height = height;
            tex_desc.MipLevels = 1;
            tex_desc.ArraySize = 1;
            tex_desc.SampleDesc.Count = 1;
            tex_desc.SampleDesc.Quality = 0;
            tex_desc.Usage = D3D11_USAGE_DEFAULT;
            tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            tex_desc.CPUAccessFlags = 0;
            tex_desc.MiscFlags = 0;

            t.create(device, tex_desc, &subdata);

            if (is_hdr)
                stbi_image_free(hdr_data);
            else
                stbi_image_free(ldr_data);
        }

        void dds_to_srv(ID3D11Device* device, const tstring& filename, ID3D11ShaderResourceView** srv)
        {
            if (S_OK != DirectX::CreateDDSTextureFromFile(device, filename.c_str(), nullptr, srv))
                throw exception(L"dds: Can't load");
        }

        tstring print_log_info(ID3D11Resource* resource)
        {
            tstringstream ret;

            ID3D11Texture2D* t2d;
            if (SUCCEEDED(resource->QueryInterface(&t2d)))
            {
                D3D11_TEXTURE2D_DESC desc;
                t2d->GetDesc(&desc);
                safe_release(t2d);

                ret << L" " << desc.Width << L"x" << desc.Height << L" " << (is_srgb(desc.Format) ? L"sRGB" : L"") << L"(F:" << desc.Format << L" M:" << desc.MipLevels << L")";
            }

            ID3D11Texture3D* t3d;
            if (SUCCEEDED(resource->QueryInterface(&t3d)))
            {
                D3D11_TEXTURE3D_DESC desc;
                t3d->GetDesc(&desc);
                safe_release(t3d);

                ret << L" " << desc.Width << L"x" << desc.Height << L"x" << desc.Depth << L" (F:" << desc.Format << L" M:" << desc.MipLevels << L")";
            }

            return ret.str();
        }

        tstring print_log_info(ID3D11ShaderResourceView* srv)
        {
            ID3D11Resource* resource;
            srv->GetResource(&resource);
            tstring ret = print_log_info(resource);
            safe_release(resource);
            return ret;
        }

        void texture_from_file(ID3D11Device* device, tstring filename, ID3D11ShaderResourceView** srv)
        {
            std::replace(filename.begin(), filename.end(), L'\\', L'/');

            bool is_dds = filename.find(L".dds") != std::string::npos;

            try
            {
                if (!is_dds)
                    stb_to_srv(device, to_string(filename).c_str(), srv);
                else
                    dds_to_srv(device, filename, srv);

                tclog << L"Loading: " << filename << print_log_info(*srv) << std::endl;
            }
            catch (std::exception& e)
            {
                tcout << L"Failed to load: " << filename << std::endl;
                tcout << e.what() << std::endl;
            }
        }
    }

    void load_texture(ID3D11Device* device, const tstring& texture_file, texture& t)
    {
        detail::stb_to_texture(device, to_string(make_absolute_path(texture_file)).c_str(), t);
    }

    void load_texture(ID3D11Device* device, const tstring& texture_file, ID3D11ShaderResourceView** srv)
    {
        if (srv)
            *srv = nullptr;

        texture_cache::i().add_texture(device, texture_file);

        if (texture_file != L"" && srv)
            *srv = texture_cache::i().srv(texture_file);
    }

    std::map<tstring, ID3D11ShaderResourceView*> texture_cache::texture_cache_;

    texture_cache& texture_cache::i()
    {
        static texture_cache t;
        return t;
    }

    void texture_cache::add_texture(ID3D11Device* device, const tstring& filename)
    {
        if (srv(filename) != 0)
            return;

        // load texture
        ID3D11ShaderResourceView* srv;
        detail::texture_from_file(device, make_absolute_path(filename).c_str(), &srv);

        texture_cache_[filename] = srv;
    }

    ID3D11ShaderResourceView* texture_cache::srv(const tstring& filename) const
    {
        auto it = texture_cache_.find(filename);

        if (it != texture_cache_.end())
            return it->second;

        return nullptr;
    }

    void texture_cache::destroy()
    {
        for (auto i = texture_cache_.begin(); i != texture_cache_.end(); ++i)
            safe_release(i->second);

        texture_cache_.clear();
    }

    void texture_cache::generate_mips(ID3D11DeviceContext* context)
    {
        // TODO
        for (auto i = texture_cache_.begin(); i != texture_cache_.end(); ++i)
        {
            //ID3D11Resource* resource;
            //i->second->GetResource(&resource);

            //D3DX11FilterTexture(context, resource, 0, D3DX11_DEFAULT);
        }
    }
}
