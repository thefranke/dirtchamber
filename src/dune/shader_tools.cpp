/*
 * Dune D3D library - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "shader_tools.h"

#include <iostream>
#include <fstream>
#include <D3Dcompiler.h>

#include "common_tools.h"
#include "d3d_tools.h"

namespace dune
{
    struct include_handler : public ID3D10Include
    {
        std::string path;

        include_handler(const std::string& filename)
        {
            path = filename;
            auto it = path.find_last_of('/');

            if (it != std::string::npos)
                path = path.substr(0, it+1);
            else
                path = "";
        }

        STDMETHOD(Open)(D3D10_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pByteLen)
        {
            std::ifstream f(path + pFileName);
            std::string fstr((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());

            *pByteLen = static_cast<UINT>(fstr.length());
            char* data = new char[*pByteLen];
            memcpy(data, fstr.c_str(), *pByteLen);
            *ppData = data;

            return S_OK;
        }

        STDMETHOD(Close)(LPCVOID pData)
        {
            char* data = (char*)pData;
            delete [] data;
            return S_OK;
        }
    };

    namespace detail
    {
        void create_blob(ID3D11Device*, LPCTSTR filename, LPCSTR mainfunc, ID3DBlob** blob)
        {
            tstring full_filename = make_absolute_path(filename);
            size_t ls = full_filename.find_last_of('/');
            full_filename = full_filename.substr(0, ls);

            tstring mf = dune::to_tstring(mainfunc);

            full_filename += L"/cso/" + mf + L".cso";

            std::ifstream ifs(full_filename, std::ios::binary);

            if (!ifs.good())
                throw exception(tstring(L"Couldn't load ") + filename);

            tclog << L"Loading: " << full_filename << std::endl;

            std::string bin((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

            assert_hr(D3DCreateBlob(bin.size(), blob));
            std::memcpy((*blob)->GetBufferPointer(), bin.c_str(), bin.size());
        }

        void compile_shader(ID3D11Device* device, LPCTSTR filename, LPCSTR shadermodel, LPCSTR mainfunc, UINT shader_flags, D3D_SHADER_MACRO* defines, ID3DBlob** binary)
        {
            tstring full_filename = make_absolute_path(filename);

            std::ifstream f(full_filename);

            // no shader file found, try loading binary
            if (!f.good())
            {
                create_blob(device, filename, mainfunc, binary);
                return;
            }

            tclog << L"Loading: " << full_filename << std::endl;

            std::string fstr((std::istreambuf_iterator<char>(f)),
                std::istreambuf_iterator<char>());

            ID3DBlob* errors = nullptr;

            include_handler ih(to_string(full_filename).c_str());
            HRESULT hr = D3DCompile(fstr.c_str(), fstr.length(), to_string(full_filename).c_str(), defines, &ih, mainfunc, shadermodel, shader_flags, 0, binary, &errors);

            if (FAILED(hr) || !binary)
            {
                char* perrors = 0;

                if (errors)
                    perrors = reinterpret_cast<char*>(errors->GetBufferPointer());

                if (perrors)
                    throw exception(tstring(L"Failed to compile shader: ") + to_tstring(std::string(perrors, errors->GetBufferSize())));

                safe_release(errors);

                throw exception(L"Failed to compile shader");
            }
        }

        void compile_shader(ID3D11Device* device, ID3DBlob* binary, ID3D11VertexShader** vs)
        {
            assert_hr(device->CreateVertexShader(static_cast<DWORD*>(binary->GetBufferPointer()), binary->GetBufferSize(), nullptr, vs));
        }

        void compile_shader(ID3D11Device* device, ID3DBlob* binary, ID3D11PixelShader** ps)
        {
            assert_hr(device->CreatePixelShader(static_cast<DWORD*>(binary->GetBufferPointer()), binary->GetBufferSize(), nullptr, ps));
        }

        void compile_shader(ID3D11Device* device, ID3DBlob* binary, ID3D11GeometryShader** gs)
        {
            assert_hr(device->CreateGeometryShader(static_cast<DWORD*>(binary->GetBufferPointer()), binary->GetBufferSize(), nullptr, gs));
        }
    }
}
