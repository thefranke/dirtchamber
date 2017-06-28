/*
 * Dune D3D library - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_KINECT_GBUFFER
#define DUNE_KINECT_GBUFFER

#ifdef MICROSOFT_KINECT_SDK

#include <D3D11.h>
#include <NuiApi.h>

#include <memory>

#include "gbuffer.h"
#include "unicode.h"

#define RT_KINECT_DEPTH L"depth"
#define RT_KINECT_COLOR L"color"

namespace dune
{
    /*!
     * \brief A GBuffer fed from a Kinect camera.
     *
     * The kinect_gbuffer is a regular Geometry Buffer which is fed from a Kinect camera. It has two targets
     * for depth and colors. When passing this buffer to a deferred renderer, it will not notice the difference
     * to a regular GBuffer and can therefore render for instance post-processing effects as usual.
     *
     * kinect_gbuffer objects work with a two step update model with pull mechanics.
     * A thread is spawned which copies images received from the Kinect into two internal render_target objects.
     * Once the render_target objects are written to, flags are raised to indicate this to other threads.
     * The application thread calls the update method to pull data over into the gbuffer
     * objects. This is necessary to be able to update buffers without managing two D3D context objects
     * in a multithreading environment, because GBuffers under normal circumstances can only be accessed
     * in the same thread as the renderer.
     */
    class kinect_gbuffer : public gbuffer
    {
    protected:
        enum DATA_TYPE
        {
            DEPTH,
            COLOR
        };

        HANDLE the_thread_;
        HANDLE color_event_;
        HANDLE depth_event_;
        HANDLE kill_event_;
        HANDLE color_stream_;
        HANDLE depth_stream_;

        INuiSensor *sensor_;

        bool align_streams_;

        CRITICAL_SECTION cs_color_, cs_depth_;

    protected:
        /*! \brief The thread which updates render_targets from the Kinect API. */
        static DWORD WINAPI thread(LPVOID);

        /*! \brief Initializes a Kinect camera. */
        void init();

        /*! \brief Stops the Kinect thread and shuts down the Kinect. */
        void shutdown();

        /*! \brief Map and update the depth render_target GPU memory from the CPU copy. */
        void update_depth(ID3D11DeviceContext* context);

        /*! \brief Map and update the color render_target GPU memory from the CPU copy. */
        void update_color(ID3D11DeviceContext* context);

        size_t width_, height_;
        bool has_new_depth_, has_new_color_;

        // hide me: kinect_gbuffer cannot be created empty or resized.
        void create(ID3D11Device* device);
        void resize(size_t width, size_t height);

        /*!
         * \brief Copies over NUI_IMAGE_FRAMEs to render_targets.
         *
         * Calling this method with a type will copy over a NUI_IMAGE_FRAME to a render_target's CPU bound buffer.
         *
         * \param type The type of the buffer to copy: color or depth.
         */
        void copy_buffer(DATA_TYPE type);

        /*! \brief Lock a buffer type. */
        void lock(DATA_TYPE type);

        /*! \brief Unlock a buffer type. */
        void unlock(DATA_TYPE type);

    public:
        kinect_gbuffer();
        virtual ~kinect_gbuffer() {}

        virtual void create(ID3D11Device* device, const tstring& name, UINT w = 640, UINT h = 480, bool align_streams = true);
        void destroy();

        /*! \brief Start an initialized Kinect camera. */
        void start();

        /*! \brief Stop a running Kinect camera. */
        void stop();

        /*! \brief Copy over all render_target CPU memory to the GPU. Call this method every before rendering. */
        void update(ID3D11DeviceContext* context);

        /*! \brief Returns the depth render_target. */
        render_target* depth()
        { return target(RT_KINECT_DEPTH); }

        /*! \brief Returns the color render_target. */
        render_target* color()
        { return target(RT_KINECT_COLOR); }

        //!@{
        /*! \brief Return the minimum and maximum depth range defined by the Kinect SDK. */
        float depth_min() const;
        float depth_max() const;
        //!@}
    };
}

#endif

#endif
