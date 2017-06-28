/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_DEFERRED_RENDERER
#define DUNE_DEFERRED_RENDERER

#include <vector>
#include <string>
#include <map>

#include "shader_resource.h"
#include "simple_mesh.h"

#include <D3D11.h>

namespace dune
{
    class texture;
    class gbuffer;
    class postprocessor;
}

namespace dune
{
    /*!
     * \brief The deferred renderer base class.
     *
     * This class implements a deferred renderer manager. A deferred_renderer is initialized
     * by "adding" render_target or gbuffer objects to it with given register numbers. These
     * objects are collected into one big buffer map. When rendering, the deferred_renderer will
     * call a deferred shader for a full screen triangle and upload all buffers in the buffer
     * map to their respective registers, which can then be used by the deferred shader.
     *
     * Next to managing a lot of texture buffers, the deferred renderer can also display
     * a small preview window of a selected buffer (an overlay) and handle a reference
     * to a postprocessor, which augments the output of the deferred shader.
     *
     * Last but not least the class features a recording function, which will capture the final
     * output of whoever comes last in the pipeline into an MP4.
     */
    class deferred_renderer
    {
    private:
        typedef std::map<UINT, texture*> buffer_collection;
        typedef std::vector<ID3D11ShaderResourceView*> srv_collection;

        buffer_collection               buffers_;
        srv_collection                  srvs_;

        INT                             buffers_slot_;
        INT                             overlay_slot_;

        ID3D11ShaderResourceView*       overlay_;

        simple_mesh                     fs_triangle_;

        ID3D11PixelShader*              ps_deferred_;
        ID3D11PixelShader*              ps_overlay_;

        sampler_state                   ss_shadows_;
        sampler_state                   ss_lpv_;

        postprocessor*                  postprocessor_;
        bool                            do_record_;

        /*! \brief Reassigns all buffers in buffers_ to their respective registers. */
        void reassign_everything(ID3D11DeviceContext* context);

        /*! \brief Clear all registers that have been assigned previously. */
        void clear_assigned(ID3D11DeviceContext* context);

        /*! \brief Render a fullscreen triangle pass. */
        void blit(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer);

        /*! \brief Returns a render_target of a given name. */
        ID3D11ShaderResourceView* target(const tstring& name) const;

    protected:
        /*! \brief Render a selected target into a small preview window on top of the backbuffer. */
        void overlay(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer);

        /*! \brief Render the deferred pass with an optional postprocessing pass. */
        void deferred(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer);

    public:
        deferred_renderer();
        virtual ~deferred_renderer() {}

        virtual void create(ID3D11Device* device);

        void set_shader(ID3D11Device* device,
                        ID3DBlob* input_binary,
                        ID3D11VertexShader* vs_deferred,
                        ID3D11PixelShader* ps_deferred,
                        ID3D11PixelShader* ps_overlay,
                        UINT start_slot,
                        UINT overlay_slot);

        virtual void destroy();

        /*!
         * \brief Capture the backbuffer and write it to an MP4 video file.
         *
         * Enable capture is a toggle function to either start or stop recording a video
         * of the rendered image using ffmpeg.
         *
         * \param device The Direct3D device.
         * \param width The width of the video (doesn't have to be the same as the backbuffers width, but should be power of 2).
         * \param height The height of the video (doesn't have to be the same as the backbuffers height, but should be power of 2).
         * \param fps The frames per second written to the video.
         */
        void start_capture(ID3D11Device* device, size_t width, size_t height, size_t fps);

        /*! \brief Stop capturing and finalize an MP4 video file. */
        void stop_capture();

        void render(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer, ID3D11DepthStencilView* dsv = nullptr);

        /*! \brief Returns the map containing all buffers indexed by their registers. */
        const buffer_collection& buffers();

        /*!
         * \brief Add a gbuffer to the deferred renderer buffer map.
         *
         * A gbuffer of e.g. three render targets starting at slot 4 will
         * upload to register t4, t5 and t6.
         *
         * \param buffer The gbuffer object.
         * \param slot The texture register starting number.
         */
        void add_buffer(gbuffer& buffer, UINT slot);

        /*! \brief Add a render_target to the buffer map. */
        void add_buffer(texture& target, UINT slot);

        /*! \brief Set a postprocessor to use. If this isn't called or set to nullptr, the deferred renderer output is displayed directly. */
        void set_postprocessor(postprocessor* pp);

        /*! \brief Select a render_target by name to display in a small overlay when rendering. */
        void show_target(ID3D11DeviceContext* context, const tstring& name);

        /*! \brief Resize the deferred_renderer's output to a new width/height .*/
        virtual void resize(UINT width, UINT height);
    };
}

#endif
