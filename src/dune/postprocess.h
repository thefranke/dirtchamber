/*
 * Dune D3D library - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_POSTPROCESS
#define DUNE_POSTPROCESS

#include "render_target.h"
#include "simple_mesh.h"

#include <list>
#include <string>

namespace dune
{
    /*!
     * \brief A base interface for a postprocessor pipeline.
     *
     * This class defines a base interface for a postprocessing pipeline. A postprocessing pipeline is activated
     * by a deferred_renderer after the deferred render pass. It receives the deferred output as initial image and
     * is expected to render its end result into a backbuffer render target view. Everything in between is
     * dependent on the pipeline and can introduce as many render steps as needed.
     */
    class postprocessor
    {
    protected:
        INT                     buffers_start_slot_;
        simple_mesh             fs_triangle_;

        render_target           frontbuffer_;

        bool                    enabled_;

        /*! \brief Overwrite this method to add your own creation code. */
        virtual void do_create(ID3D11Device* device)
        {
        }

        /*! \brief Overwrite this to destroy additional resources created. */
        virtual void do_destroy()
        {
        }

        /*! \brief Overwrite this method to initializes further shader instances. */
        virtual void do_set_shader(ID3D11Device* device)
        {
        }

        /*! \brief Overwrite this method to react to resizes. */
        virtual void do_resize(UINT width, UINT height)
        {
        }

    public:
        postprocessor();
        virtual ~postprocessor() {}

        void create(ID3D11Device* device, DXGI_SURFACE_DESC desc);

        /*! \brief Render the entire postprocess (i.e. all steps of the pipeline) and write the result to backbuffer. */
        virtual void render(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer);

        /*!
         * \brief Set shader parameters for the postprocessing pipeline.
         *
         * The postprocessing pipeline needs several things in order to run: A fullscreen triangle with
         * the matching shader input_binary and a start slot at which point to expect the frontbuffer (i.e
         * the buffer into which the image just before postprocessing was rendered).
         *
         * \param device The Direct3D device.
         * \param input_binary The input layout of the shader for the fullscreen triangle.
         * \param fs_triangle A vertex shader for a fullscreen triangle.
         * \param buffers_start_slot The slot at which the shader expects the frontbuffer.
         */
        void set_shader(ID3D11Device* device, ID3DBlob* input_binary, ID3D11VertexShader* fs_triangle, UINT buffers_start_slot);

        void destroy();

        /*! \brief Resizes the frontbuffer into which a deferred renderer writes its image to. */
        void resize(UINT width, UINT height);

        /*! \brief Render into this render_target for the postprocessor. */
        inline render_target& frontbuffer() { return frontbuffer_; }

        /*! \brief Enable the postprocessing pipeline. */
        inline void enable()  { enabled_ = true; };

        /*! \brief Disable the postprocessing pipeline. */
        inline void disable() { enabled_ = false; };

        /*! \brief Check whether or not the postprocessing pipeline is enabled. */
        inline bool enabled() { return enabled_; }
    };
}

#endif
