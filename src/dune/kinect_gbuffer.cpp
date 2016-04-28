/*
 * Dune D3D library - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "kinect_gbuffer.h"

#ifdef MICROSOFT_KINECT_SDK

#include "d3d_tools.h"
#include "math_tools.h"
#include "exception.h"
#include "common_tools.h"

#include <cassert>
#include <iostream>
#include <sstream>

namespace dune
{
    namespace detail
    {
        template<typename T>
        void copy_flipped_h(T* input, T* output, size_t width, size_t height, size_t channels)
        {
            for (size_t y = 0; y < height; ++y)
            for (size_t x = 0; x < width; ++x)
            {
                size_t i        = ((y * width) + x) * channels;
                size_t flip_i   = ((y * width) + (width - x - 1)) * channels;

                for (size_t c = 0; c < channels; ++c)
                    output[i+c] = input[flip_i+c];
            }
        }

        template<typename T>
        void copy_flipped_v(T* input, T* output, size_t width, size_t height, size_t channels)
        {
            for (size_t y = 0; y < height; ++y)
            for (size_t x = 0; x < width; ++x)
            {
                size_t i        = ((y * width) + x) * channels;
                size_t flip_i   = (((height - y - 1) * width) + x) * channels;

                for (size_t c = 0; c < channels; ++c)
                    output[i+c] = input[flip_i+c];
            }
        }

        // see http://graphics.stanford.edu/~mdfisher/Kinect.html
        float depth_to_m(USHORT depth_value)
        {
            return static_cast<float>(depth_value)/1000.f;
        }

        DirectX::XMFLOAT3 depth_to_world(int x, int y, int depth_value)
        {
            static const double fx_d = 1.0 / 5.9421434211923247e+02;
            static const double fy_d = 1.0 / 5.9104053696870778e+02;
            static const double cx_d = 3.3930780975300314e+02;
            static const double cy_d = 2.4273913761751615e+02;

            DirectX::XMFLOAT3 result;

            // get depth in meters
            const double depth = depth_to_m(depth_value);

            result.x = float((x - cx_d) * depth * fx_d);
            result.y = float((y - cy_d) * depth * fy_d);
            result.z = float(depth);

            return result;
        }
    }

    kinect_gbuffer::kinect_gbuffer() :
        the_thread_(),
        color_event_(),
        depth_event_(),
        kill_event_(),
        color_stream_(),
        depth_stream_(),
        sensor_(nullptr),
        align_streams_(false),
        cs_color_(),
        cs_depth_()
    {
    }

    void kinect_gbuffer::init()
    {
        int device_id_ = 0;

        assert_hr(NuiCreateSensorByIndex(device_id_, &sensor_));

        DWORD options = NUI_INITIALIZE_FLAG_USES_COLOR; // | NUI_INITIALIZE_FLAG_USES_DEPTH;

        assert_hr(sensor_->NuiInitialize(options));

        NUI_IMAGE_RESOLUTION eResolution = NUI_IMAGE_RESOLUTION_640x480;

        if (width_ == 640 && height_ == 480)
            eResolution = NUI_IMAGE_RESOLUTION_640x480;
        else if (width_ == 1280 && height_ == 960)
            eResolution = NUI_IMAGE_RESOLUTION_1280x960;
        else
        {
            tcout << L"Invalid resolution, setting back to 640x480" << std::endl;
            width_ = 640;
            height_ = 480;
        }

        color_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        depth_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        kill_event_  = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        assert_hr(sensor_->NuiImageStreamOpen(
            NUI_IMAGE_TYPE_COLOR, eResolution,
            0,
            2,
            color_event_,
            &color_stream_));

        if ((options & NUI_INITIALIZE_FLAG_USES_DEPTH) != 0)
        {
            // fixed size
            assert_hr(sensor_->NuiImageStreamOpen(
                NUI_IMAGE_TYPE_DEPTH,
                NUI_IMAGE_RESOLUTION_640x480,
                0,
                2,
                depth_event_,
                &depth_stream_));

            assert_hr(sensor_->NuiImageStreamSetImageFrameFlags(depth_stream_, NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE));
        }

        has_new_color_ = has_new_depth_ = false;
    }

    float kinect_gbuffer::depth_min() const
    {
        DWORD flags = NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE;
        if (S_OK == sensor_->NuiImageStreamGetImageFrameFlags(depth_stream_, &flags))
            return NUI_IMAGE_DEPTH_MINIMUM_NEAR_MODE;
        else
            return NUI_IMAGE_DEPTH_MINIMUM;
    }

    float kinect_gbuffer::depth_max() const
    {
        DWORD flags = NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE;
        if (S_OK == sensor_->NuiImageStreamGetImageFrameFlags(depth_stream_, &flags))
            return NUI_IMAGE_DEPTH_MAXIMUM_NEAR_MODE;
        else
            return NUI_IMAGE_DEPTH_MAXIMUM;
    }

    void kinect_gbuffer::shutdown()
    {
        if (sensor_)
        {
            sensor_->NuiShutdown();
            safe_release(sensor_);
        }

        if (kill_event_)
        {
            CloseHandle(kill_event_);
            kill_event_ = nullptr;
        }

        if (color_event_)
        {
            CloseHandle(color_event_);
            color_event_ = nullptr;
        }

        if (depth_event_)
        {
            CloseHandle(depth_event_);
            depth_event_ = nullptr;
        }

        DeleteCriticalSection(&cs_color_);
        DeleteCriticalSection(&cs_depth_);
    }

    void kinect_gbuffer::lock(DATA_TYPE type)
    {
        if (type == COLOR)
            EnterCriticalSection(&cs_color_);
        else
            EnterCriticalSection(&cs_depth_);
    }

    void kinect_gbuffer::unlock(DATA_TYPE type)
    {
        if (type == COLOR)
            LeaveCriticalSection(&cs_color_);
        else
            LeaveCriticalSection(&cs_depth_);
    }

    void kinect_gbuffer::copy_buffer(DATA_TYPE type)
    {
        lock(type);

        NUI_IMAGE_FRAME image_frame;

        HANDLE stream;
        tstring stream_name;
        if (type == COLOR)
        {
            stream = color_stream_;
            stream_name = L"color";
        }
        else
        {
            stream = depth_stream_;
            stream_name = L"depth";
        }

        if (FAILED(sensor_->NuiImageStreamGetNextFrame(stream, 100, &image_frame)))
        {
            tcout << L"Can't fetch " << stream_name << L" image" << std::endl;
            return;
        }

        INuiFrameTexture* texture = image_frame.pFrameTexture;

        NUI_LOCKED_RECT locked_rect;
        if (FAILED(texture->LockRect(0, &locked_rect, nullptr, 0)))
        {
            tcout << L"Could not unlock " << stream_name << L" texture" << std::endl;
            return;
        }

        if (locked_rect.Pitch != 0)
        {
            BYTE* data = reinterpret_cast<BYTE*>(locked_rect.pBits);

            if (type == COLOR)
            {
                render_target* t = color();

                BYTE* target = t->data<BYTE*>();
                size_t color_bytes = t->desc().Width * t->desc().Height * 4;

                // TODO: broken, fixme
                /*
                if (align_streams_)
                {
                    BYTE* target_depth = depth()->data<BYTE*>();

                    ZeroMemory(target, sizeof(BYTE) * color_bytes);

                    for (UINT y = 0; y < static_cast<UINT>(height_); ++y)
                    for (UINT x = 0; x < static_cast<UINT>(width_); ++x)
                    {
                        UINT i = ((y*static_cast<UINT>(width_)) + x);

                        USHORT d = target_depth[i];

                        LONG nx, ny;

                        static NUI_IMAGE_VIEW_AREA va;
                        ZeroMemory(&va, sizeof(NUI_IMAGE_VIEW_AREA));

                        // shift d for better coordinate transform
                        if (S_OK == sensor_->NuiImageGetColorPixelCoordinatesFromDepthPixelAtResolution(
                            NUI_IMAGE_RESOLUTION_640x480, NUI_IMAGE_RESOLUTION_640x480, &va, x, y, d, &nx, &ny))
                        {
                            UINT j = ((ny*static_cast<UINT>(width_)) + nx) * 4;
                            UINT k = i * 4;

                            // pixels may lie outside of bounds, check first
                            if (nx < 640 && ny < 480 && d != 0)
                                for (size_t c = 0; c < 4; ++c)
                                    target[k + c] = data[j + c];
                        }
                    }
                }
                else*/
                    std::memcpy(target, data, sizeof(BYTE) * color_bytes);

                has_new_color_ = true;
            }
            else
            {
                //std::memcpy(&raw_data_depth_[0], data, raw_data_depth_.size() * sizeof(USHORT));
                render_target* t = depth();
                std::memcpy(t->data<BYTE*>(), data, sizeof(USHORT) * t->desc().Width * t->desc().Height);
                has_new_depth_ = true;
            }

            texture->UnlockRect(0);
        }

        sensor_->NuiImageStreamReleaseFrame(stream, &image_frame);

        unlock(type);
    }

    DWORD WINAPI kinect_gbuffer::thread(LPVOID data)
    {
        assert(data);
        kinect_gbuffer* o = reinterpret_cast<kinect_gbuffer*>(data);

        while(true)
        {
            HANDLE events[] = { o->color_event_, o->depth_event_, o->kill_event_ };

            DWORD event_idx = WaitForMultipleObjects(3, events, FALSE, 100);

            // only set flags that new data is ready
            // used to avoid thread confusion in D3D: the render target
            // can be updated in the same thread as the rendering code
            switch(event_idx)
            {
            case WAIT_OBJECT_0:
                o->copy_buffer(COLOR);
                break;
            case WAIT_OBJECT_0 + 1:
                o->copy_buffer(DEPTH);
                break;
            default:
            case WAIT_TIMEOUT:
                break;
            }

            // received kill event, stop thread execution now
            if (event_idx == WAIT_OBJECT_0 + 2)
                break;
        }

        return 0;
    }

    void kinect_gbuffer::create(ID3D11Device* device, const tstring& name, UINT width, UINT height, bool align_streams)
    {
        assert(device);

        device_ = device;
        width_ = width;
        height_ = height;
        align_streams_ = align_streams;

        init();
        gbuffer::create(device, name);

        InitializeCriticalSection(&cs_color_);
        InitializeCriticalSection(&cs_depth_);

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = static_cast<UINT>(width_);
        desc.Height = static_cast<UINT>(height_);
        desc.MipLevels = desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        add_target(RT_KINECT_COLOR, desc, true);

        desc.Width = 640;
        desc.Height = 480;
        desc.Format = DXGI_FORMAT_R32_FLOAT;
        add_target(RT_KINECT_DEPTH, desc, true);
    }

    void kinect_gbuffer::destroy()
    {
        stop();
        shutdown();
        gbuffer::destroy();
    }

    void kinect_gbuffer::start()
    {
        the_thread_ = CreateThread(
            nullptr,
            0,
            thread,
            this,
            0,
            nullptr);
    }

    void kinect_gbuffer::stop()
    {
        // send kill
        SetEvent(kill_event_);
        WaitForSingleObject(kill_event_, INFINITE);

        unlock(COLOR);
        unlock(DEPTH);
        CloseHandle(the_thread_);
    }

    void kinect_gbuffer::update_color(ID3D11DeviceContext* context)
    {
        lock(COLOR);
        {
            color()->update(context);
        }
        unlock(COLOR);
    }

    void kinect_gbuffer::update_depth(ID3D11DeviceContext* context)
    {
        lock(DEPTH);
        {
            depth()->update(context);
        }
        unlock(DEPTH);
    }

    void kinect_gbuffer::update(ID3D11DeviceContext* context)
    {
        if (has_new_color_)
        {
            update_color(context);
        }

        if (has_new_depth_)
        {
            update_depth(context);
        }

        // reset state
        has_new_depth_ = has_new_color_ = false;
    }
}

#endif
