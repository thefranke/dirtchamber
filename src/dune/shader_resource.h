/* 
 * dune::shader_resource by Tobias Alexander Franke (tob@cyberhead.de) 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_SHADER_RESOURCE
#define DUNE_SHADER_RESOURCE

#include <D3D11.h>
#include "unicode.h"

namespace dune 
{
    /*!
     * \brief A shader resource wrapper.
     *
     * This basic interface defines a resource which can be anything that can be uploaded to a GPU.
     * Each shader_resource has an identifying name, a function to destroy it and free all its resources
     * and several functions to upload it to a typed shader into a specified register.
     */
    struct shader_resource
    {
        tstring name;

        /*! \brief Upload the shader resource to register slot of a vertex shader. */
        virtual void to_vs(ID3D11DeviceContext* context, UINT slot);

        /*! \brief Upload the shader resource to register slot of a geometry shader. */
        virtual void to_gs(ID3D11DeviceContext* context, UINT slot);

        /*! \brief Upload the shader resource to register slot of a pixel shader. */
        virtual void to_ps(ID3D11DeviceContext* context, UINT slot);

        /*! \brief Upload the shader resource to register slot of a compute shader. */
        virtual void to_cs(ID3D11DeviceContext* context, UINT slot);

        /*! \brief Destroy the shader_resource and free all memory. */
        virtual void destroy() = 0;
    };

    /*!
     * \brief A wrapper class for a sampler state.
     *
     * Manages an ID3D11SamplerState.
     */
    struct sampler_state : public shader_resource
    {
        ID3D11SamplerState* state;

        void create(ID3D11Device* device, const D3D11_SAMPLER_DESC& desc);
        void destroy();

        void to_vs(ID3D11DeviceContext* context, UINT slot);
        void to_ps(ID3D11DeviceContext* context, UINT slot);
    };
}

#endif