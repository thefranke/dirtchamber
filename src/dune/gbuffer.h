/* 
 * dune::gbuffer by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_GBUFFER
#define DUNE_GBUFFER

#include "render_target.h"

#include <vector>
#include <string>

#include <D3D11.h>

namespace dune 
{
    /*!
     * \brief A geometry buffer (collection of render_target objects).
     *
     * This class is a map of named render_target objects. The name gbuffer is only there because it isn't used as
     * a simple collection anywhere in the Dirtchamber. Each gbuffer can hold 0-N targets which are either
     * added directly, or with helper functions called add_target or _add_depth. Resizing a gbuffer
     * resizes all targets in it, and it is assumed that targets are all of equal size (even though
     * they don't have to be).
     */
    class gbuffer : public shader_resource
    {
    protected:
        typedef std::vector<render_target> targets_vec;
        typedef std::vector<ID3D11ShaderResourceView*> srv_vec;
    
        srv_vec                 srvs_;
        targets_vec             buffers_;
        ID3D11Device*           device_;
    
    protected:
        //!@{
        /*! \brief Return a named target. */
        render_target* target(const tstring& name);
        const render_target* target(const tstring& name) const;
        //!@}

        /*! \brief Fill up a vector of ID3D11ShaderResourceView with the SRVs of all render_target objects in this gbuffer. */
        void srvs(srv_vec& srv_array);

    public:
        gbuffer();
        virtual ~gbuffer() {}

        void create(ID3D11Device* device, const tstring& name);
        virtual void destroy();

        //!@{
        /*! \brief Create a new render_target in the gbuffer. These functions basically forward parameters to the render_target's create function. */
        void add_target(const tstring& name, render_target& target);
        void add_target(const tstring& name, const D3D11_TEXTURE2D_DESC& desc, bool cached = false);
        void add_target(const tstring& name, const DXGI_SURFACE_DESC& desc, UINT num_mipmaps = 1);
        void add_depth(const tstring& name, DXGI_SURFACE_DESC desc);
        //!@}
    
        /*! \brief Returns all render_target objects in this gbuffer. */
        targets_vec& targets();

        //!@{
        /*! \brief Fetch a render_target with its name. */
        render_target* operator[](const tstring& name);
        const render_target* operator[](const tstring& name) const;
        //!@}

        /*! \brief Resize all render_target objects in this gbuffer to a new width and height. */
        void resize(UINT width, UINT height);

        void to_ps(ID3D11DeviceContext* context, UINT start_slot);
        void to_gs(ID3D11DeviceContext* context, UINT start_slot);
        void to_vs(ID3D11DeviceContext* context, UINT start_slot);
    };
} 

#endif