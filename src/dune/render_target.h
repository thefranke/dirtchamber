/* 
 * dune::render_target by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_RENDER_TARGET
#define DUNE_RENDER_TARGET

#include <D3D11.h>
#include <DirectXMath.h>
#include <string>

#include "texture.h"

namespace dune 
{
    /*!
     * \brief A render target wrapper.
     *
     * A render target is a texture that can be "written" to from the outside, whether it be from
     * the GPU or the CPU. A render_target object can be created in several different manners
     * (from manuel specification to supplying common descriptors) and provides functions
     * to query views later used to read from or render to it. Furthermore, depending on the 
     * parameters supplied to create(), render_target objects can also be CPU mapped to read
     * or write from/to them.
     */
    class render_target : public texture
    {
    protected:
        ID3D11Device* device_;
        ID3D11RenderTargetView* rtview_;
        ID3D11DepthStencilView* dsview_;

        bool cached_;
        std::vector<BYTE> cached_copy_;

    protected:
        virtual void do_create(ID3D11Device* device, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA* subresource);

    public:
        render_target();
        virtual ~render_target() {}

        /* \brief Returns the render target view (RTV). */
        ID3D11RenderTargetView* const rtv() const { return rtview_; }

        /* \brief Returns the depth stencil view (DSV). */
        ID3D11DepthStencilView*     const dsv() const { return dsview_; }

        /*
         * \brief Resize the render_target.
         *
         * Resizes the render_target. Because new views are generated, old views still bound to the GPU are invalid. After a resize,
         * all views need to be re-bound!
         *
         * \param width The new width of the render_target.
         * \param height The new height of the render_target.
         */
        void resize(UINT width, UINT height);

        virtual void destroy();

        /*! \brief If this texture is cached, this method will copy the CPU side cache to the render_target texture. */
        void update(ID3D11DeviceContext* context);

        /*! \brief Copy a data array of num_bytes size into the internal CPU cache. */
        void copy(const BYTE* data, size_t num_bytes);

        /*! \brief Returns a pointer to the internal CPU texture cache. */
        template<typename T>
        T data()
        {
            return reinterpret_cast<T>(&cached_copy_[0]);
        }

        //!@{
        /*! \brief Enable caching. If this is true, creating this object will copy the initial subresource to a cache. */
        void enable_cached(bool c) { cached_ = c;  }
        bool cached() { return cached_; }
        //!@}
    };

    void load_static_target(ID3D11Device* device, const tstring& filename, render_target& t);
}

#endif // RENDER_TARGET