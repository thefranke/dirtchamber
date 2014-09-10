/* 
 * dune::shader_tools by Tobias Alexander Franke (tob@cyberhead.de) 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_SHADER_TOOLS
#define DUNE_SHADER_TOOLS

#include <map>
#include <string>

#include <D3D11.h>

#include "exception.h"

namespace dune 
{
    namespace detail
    {
        void compile_shader(ID3D11Device* device, LPCTSTR filename, LPCSTR shadermodel, LPCSTR mainfunc, UINT shader_flags, D3D_SHADER_MACRO* defines, ID3DBlob** binary);
        void compile_shader(ID3D11Device* device, ID3DBlob* binary, ID3D11GeometryShader** gs);
        void compile_shader(ID3D11Device* device, ID3DBlob* binary, ID3D11PixelShader** ps);
        void compile_shader(ID3D11Device* device, ID3DBlob* binary, ID3D11VertexShader** vs);
    }

    /*!
     * \brief (Re-)Compile a shader.
     *
     * This function tries to compile a shader and returns the D3D shader object and a possible binary blob which contains layout information.
     * Includes in HLSL are handled automatically, and error handling is done via the Dirtchamber logging mechanism. First, the shader file
     * is loaded and compiled to a blob. If this didn't work, the blob is discarded and an error is written to the logger. Otherwise, the
     * blob is turned to a proper shader and whatever shader pointer was supplied is now cleaned up and exchanged with a new shader.
     *
     * If the shader file was not found, but a file cso/mainfunc.cso instead was found, this precompiled version will be loaded instead and no
     * error is logged.
     *
     * \param device The Direct3D device.
     * \param filename A shader filename.
     * \param shadermodel An identifier of the shader model used, e.g. vs_5_0.
     * \param mainfunc The shader entry point.
     * \param shader_flags Any shader flags such as D3DCOMPILE_ENABLE_STRICTNESS.
     * \param defines HLSL defines.
     * \param shader A pointer to an ID3D11*Shader, which can already hold a valid shader object which will be replaced.
     * \param binary An optional pointer to an ID3D11Blob, which will contain the blob of the shader if it compiles.
     */
    template<typename T>
    void compile_shader(ID3D11Device* device, LPCTSTR filename, LPCSTR shadermodel, LPCSTR mainfunc, UINT shader_flags, D3D_SHADER_MACRO* defines, T** shader, ID3DBlob** binary = nullptr)
    {
        ID3DBlob* blob = nullptr;

        try
        {
            detail::compile_shader(device, filename, shadermodel, mainfunc, shader_flags, defines, &blob);
        }
        catch (exception& e)
        {
            tcerr << e.msg() << std::endl;
            safe_release(blob);
            return;
        }
      
        safe_release(*shader);

        try 
        { 
            detail::compile_shader(device, blob, shader);
        }
        catch (exception& e)
        {
            safe_release(blob);
            throw e;
        }

        if (binary)
            *binary = blob;
        else
            safe_release(blob);
    }
}

#endif