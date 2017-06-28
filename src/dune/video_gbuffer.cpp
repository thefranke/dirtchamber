/*
 * Dune D3D library - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "video_gbuffer.h"

#include <cassert>

#include "unicode.h"
#include "d3d_tools.h"

#pragma comment(lib, "strmiids")

namespace dune
{
    namespace detail
    {
        struct video_device
        {
            tstring         name;
            IBaseFilter*    filter;

            void destroy()
            {
                safe_release(filter);
            }
        };

        struct video_device_list
        {
            std::vector<video_device> devices;

            HRESULT create(IFilterGraph2* graph)
            {
                // FIXME: do not assert here -> result is RCP_E_CHANGED_MODE, which can be ignored
                (CoInitialize(nullptr));

                VARIANT name;
                //LONGLONG start=MAXLONGLONG, stop=MAXLONGLONG;

                devices.clear();

                //create an enumerator for video input devices
                ICreateDevEnum*    dev_enum;
                assert_hr(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&dev_enum));

                IEnumMoniker* enum_moniker;
                HRESULT hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enum_moniker, 0);

                // no devices
                if (hr == S_FALSE)
                    return S_OK;

                //get devices
                IMoniker* moniker;
                unsigned long dev_count;
                const int MAX_DEVICES = 8;
                assert_hr(enum_moniker->Next(MAX_DEVICES, &moniker, &dev_count));

                for (size_t i=0; i < dev_count; ++i)
                {
                    //get properties
                    IPropertyBag* pbag;
                    hr = moniker[i].BindToStorage(nullptr, nullptr, IID_IPropertyBag, (void**)&pbag);

                    if (hr >= 0)
                    {
                        VariantInit(&name);

                        //get the description
                        if(FAILED(pbag->Read(L"Description", &name, 0)))
                            hr = pbag->Read(L"FriendlyName", &name, 0);

                        if (SUCCEEDED(hr))
                        {
                            //Initialize the VideoDevice struct
                            video_device dev;
                            dev.name = name.bstrVal;

                            //add a filter for the device
                            if (SUCCEEDED(graph->AddSourceFilterForMoniker(moniker+i, 0, dev.name.c_str(), &dev.filter)))
                                devices.push_back(dev);
                        }

                        VariantClear(&name);
                        safe_release(pbag);
                    }

                    moniker[i].Release();
                }

                return S_OK;
            }

            void destroy()
            {
                for (size_t i = 0; i < devices.size(); ++i)
                    devices[i].destroy();
            }
        };
    }

    video_gbuffer::video_gbuffer() :
        cs_(),
        async_(false),
        context_(nullptr),
        running_(false),
        temp_buffer_(),
        graph_(nullptr),
        capture_(nullptr),
        control_(nullptr),
        nullf_(nullptr),
        sgf_(nullptr),
        current_device_(nullptr),
        sg_(nullptr),
        callback_(),
        external_callback_(nullptr),
        has_new_buffer_(false)
    {
    }

    HRESULT __stdcall video_gbuffer::grabber_callback::SampleCB(double time, IMediaSample* sample)
    {
        if (!caller->running_)
            return S_OK;

        EnterCriticalSection(&caller->cs_);
        {
            BYTE* buffer = nullptr;
            if (FAILED(sample->GetPointer(&buffer)))
            {
                LeaveCriticalSection(&caller->cs_);
                return E_FAIL;
            }

            size_t is = sample->GetActualDataLength();
            size_t os = caller->temp_buffer_.size();

            if (os >= is)
                std::memcpy(&caller->temp_buffer_[0], buffer, is);
            else
            {
                size_t num_pixels = is/4;
                UINT ds = static_cast<UINT>(is/os);

                // downsample to half
                if (ds == 4)
                {
                    auto d = caller->target(RT_VIDEO_COLOR)->size();
                    UINT width = static_cast<UINT>(d.x);
                    UINT height = static_cast<UINT>(d.y);

                    for (UINT y = 0; y < height; ++y)
                    for (UINT x = 0; x < width; ++x)
                    {
                        UINT index_o = (y*width + x)*4;
                        UINT index_i = (y*width*ds + x*2)*4;

                        for (size_t c = 0; c < 4; ++c)
                            caller->temp_buffer_[index_o+c] = buffer[index_i+c];
                    }
                }

                // downsample to quarter
                if (ds == 16)
                {
                    auto d = caller->target(RT_VIDEO_COLOR)->size();
                    UINT width = static_cast<UINT>(d.x);
                    UINT height = static_cast<UINT>(d.y);

                    for (UINT y = 0, y1 = 0; y < height; ++y, y1+=4)
                    for (UINT x = 0, x1 = 0; x < width; ++x,  x1+=4)
                    {
                        UINT index_o = (y*width + x)*4;
                        UINT index_i = (y*width*ds + x*4)*4;

                        for (UINT c = 0; c < 4; ++c)
                            caller->temp_buffer_[index_o+c] = buffer[index_i+c];
                    }
                }
            }
        }

        caller->has_new_buffer_ = true;

        LeaveCriticalSection(&caller->cs_);

        return S_OK;
    }

    HRESULT __stdcall video_gbuffer::grabber_callback::BufferCB(double time, BYTE* buffer, long len)
    {
        return S_OK;
    }

    HRESULT __stdcall video_gbuffer::grabber_callback::QueryInterface( REFIID iid, LPVOID *ppv )
    {
        return dynamic_cast<ISampleGrabberCB*>(this)->QueryInterface(iid, ppv);
    }

    ULONG    __stdcall video_gbuffer::grabber_callback::AddRef()
    {
        return 1;
    }

    ULONG    __stdcall video_gbuffer::grabber_callback::Release()
    {
        return 0;
    }

    void video_gbuffer::set_callback(video_cb cb)
    {
        external_callback_ = cb;
    }

    HRESULT video_gbuffer::init_device(UINT32 index, size_t w, size_t h, bool async)
    {
        // init com
        // FIXME: do not assert here -> result is RCP_E_CHANGED_MODE, which can be ignored
        (CoInitialize(nullptr));

        // create graph
        assert_hr(CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IFilterGraph2, (void**)&graph_));
        assert_hr(CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&capture_));
        assert_hr(graph_->QueryInterface(IID_IMediaControl, (void**) &control_));
        assert_hr(capture_->SetFiltergraph(graph_));

        // create null renderer to suppress window output
        assert_hr(CoCreateInstance(CLSID_NullRenderer, nullptr, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&nullf_));
        assert_hr(graph_->AddFilter(nullf_, L"Null Renderer"));

        // enumerate devices
        detail::video_device_list list;
        assert_hr(list.create(graph_));

        for (size_t i = 0; i < list.devices.size(); ++i)
        {
            tclog << "Camera found: " << list.devices[i].name << std::endl;
        }

        // create samplegrabber
        assert_hr(CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&sgf_));
        assert_hr(graph_->AddFilter(sgf_, L"Sample Grabber"));
        assert_hr(sgf_->QueryInterface(IID_ISampleGrabber, (void**)&sg_));

        // set media type to RGB24
        AM_MEDIA_TYPE mt;
        ZeroMemory(&mt, sizeof(mt));
        mt.majortype = MEDIATYPE_Video;
        mt.subtype   = MEDIASUBTYPE_RGB32;
        assert_hr(sg_->SetMediaType(&mt));

        // init callback
        callback_.caller = this;
        assert_hr(sg_->SetCallback(dynamic_cast<ISampleGrabberCB*>(&callback_),0));

        // select device
        detail::video_device *dev = &list.devices[index];

        if (!dev->filter)
            return E_INVALIDARG;

        if (running_)
            stop();

        //remove and add the filters to force disconnect of pins
        if (current_device_)
        {
            graph_->RemoveFilter(current_device_);
            graph_->RemoveFilter(sgf_);

            //graph_->AddFilter(sgf_, L"Sample Grabber");
            //graph_->AddFilter(current->filter, current->filtername);
        }

        current_device_ = dev->filter;

        // set size
        /*
        IAMStreamConfig *config = nullptr;
        assert_hr(capture_->FindInterface(&PIN_CATEGORY_PREVIEW, 0, current_device_, IID_IAMStreamConfig, (void**)&config));

        int resolutions, size;
        VIDEO_STREAM_CONFIG_CAPS caps;

        AM_MEDIA_TYPE *mt1;
        config->GetFormat(&mt1);

        VIDEOINFOHEADER *info =
                        reinterpret_cast<VIDEOINFOHEADER*>(mt1->pbFormat);
        info->bmiHeader.biWidth = w;
        info->bmiHeader.biHeight = h;
        info->bmiHeader.biSizeImage = DIBSIZE(info->bmiHeader);

        assert_hr(config->SetFormat(mt1));
        */

        external_callback_ = nullptr;

        return S_OK;
    }

    void video_gbuffer::create(ID3D11Device* device, UINT32 index, const tstring& name, UINT w, UINT h)
    {
        async_ = false;
        context_ = nullptr;
        running_ = false;

        gbuffer::create(device, name);

        InitializeCriticalSection(&cs_);

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        // needed for multithreading
        //desc.MiscFlags = D3D11_BIND_UNORDERED_ACCESS;

        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        add_target(RT_VIDEO_COLOR, desc);

        temp_buffer_.resize(w * h * 4);

        // TODO: make public, independent of create for video file input?
        assert_hr(init_device(index, w, h, true));
    }

    void video_gbuffer::destroy()
    {
        stop();
        DeleteCriticalSection(&cs_);
        gbuffer::destroy();
    }

    HRESULT video_gbuffer::start(ID3D11DeviceContext* context)
    {
        context_ = context;
        running_ = true;
        has_new_buffer_ = false;

        // connect
        #ifdef SHOW_DEBUG_RENDERER
        assert_hr(capture->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, current->filter, samplegrabberfilter, nullptr));
        #else
        assert_hr(capture_->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, current_device_, sgf_, nullf_));
        #endif

        // start the riot
        LONGLONG v = MAXLONGLONG;
        assert_hr(capture_->ControlStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, current_device_, &v, &v, 1,2));

        assert_hr(control_->Run());

        return S_OK;
    }

    void video_gbuffer::stop()
    {
        running_ = false;
        has_new_buffer_ = false;

        if (control_)
            control_->StopWhenReady();

        context_ = nullptr;
    }

    void video_gbuffer::update()
    {
        if (!has_new_buffer_)
            return;

        EnterCriticalSection(&cs_);
        {
            if (external_callback_)
            {
                DirectX::XMFLOAT2 s = target(RT_VIDEO_COLOR)->size();
                external_callback_(&temp_buffer_[0], static_cast<size_t>(s.x), static_cast<size_t>(s.y));
            }

            auto rtarget = target(RT_VIDEO_COLOR);

            void* p = rtarget->map(context_);

            if (p)
            {
                // TODO: if pitch is negative, flip
                BYTE* out = reinterpret_cast<BYTE*>(p);
                std::memcpy(out, &temp_buffer_[0], temp_buffer_.size());
                rtarget->unmap(context_);
            }
        }

        has_new_buffer_ = false;

        LeaveCriticalSection(&cs_);
    }
}
