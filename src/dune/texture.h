/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_TEXTURE
#define DUNE_TEXTURE

#include <D3D11.h>
#include <DirectXMath.h>
#include <vector>

#include "shader_resource.h"

namespace dune
{
    /*!
     * \brief Wrapper for a Direct3D texture object
     *
     * Wraps up a Direct3D texture object with its own shader resource view.
     */
    class texture : public shader_resource
    {
    protected:
        ID3D11ShaderResourceView*   srview_;
        ID3D11Texture2D*            texture_;
        DirectX::XMFLOAT2           size_;

    protected:
        virtual void do_create(ID3D11Device* device, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA* subresource);

    public:
        texture();
        virtual ~texture() {}

        /* \brief Returns the shader resource view (SRV). */
        ID3D11ShaderResourceView* const srv() const { return srview_; }

        void to_vs(ID3D11DeviceContext* context, UINT slot);
        void to_ps(ID3D11DeviceContext* context, UINT slot);

        //!@{
        /*! \brief Create an empty texture from given descriptor, other resource or single parameters. */
        void create(ID3D11Device* device, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA* subresource = nullptr);
        void create(ID3D11Device* device, const DXGI_SURFACE_DESC& desc, UINT num_mipmaps = 1);
        void create(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format, const DXGI_SAMPLE_DESC& msaa, UINT num_mipmaps = 1);
        //!@}

        virtual void destroy();

        /*!
         * \brief Map the texture to a pointer.
         *
         * Maps the texture to a pointer. If the texture is CPU accessible, this pointer can be read from and/or written to.
         *
         * \param context A Direct3D context.
         * \return A pointer to the texture buffer.
         */
        void* map(ID3D11DeviceContext* context);

        /* \brief Unmap a previously mapped texture. */
        void unmap(ID3D11DeviceContext* context);

        /* \brief Returns the size of the texture as float2(width, height). */
        DirectX::XMFLOAT2 size() const;

        /* \brief Returns a texture descriptor of the texture. */
        D3D11_TEXTURE2D_DESC desc() const;

        /* \brief Returns a pointer to the actual resource behind the texture. */
        ID3D11Texture2D* resource() const;
    };
}

#endif
