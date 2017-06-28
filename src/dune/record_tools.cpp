/*
 * Dune D3D library - Tobias Alexander Franke 2017
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "record_tools.h"

#include <ctime>
#include <iomanip>

#include "d3d_tools.h"
#include "common_tools.h"
#include "gbuffer.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ext/stb/stb_image_write.h"

namespace dune
{
    video_recorder::video_recorder() :
        width_(0),
        height_(0),
        fps_(0),
        ffmpeg_texture_(0),
        ffmpeg_(0),
        path_(L"")
    {}

    video_recorder::~video_recorder()
    {}

    void video_recorder::create(ID3D11Device* device, UINT width, UINT height, UINT fps, const tstring& path)
    {
        width_ = width;
        height_ = height;
        fps_ = fps;
        path_ = path;

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;

        if (device->CreateTexture2D(&desc, nullptr, &ffmpeg_texture_) != S_OK)
        {
            tcerr << L"Failed to create staging texture for recording" << std::endl;
            return;
        }
    }

    void video_recorder::start_recording()
    {
        if (!ffmpeg_texture_)
            return;

        std::time_t t = std::time(nullptr);

        std::tm tm;
        localtime_s(&tm, &t);

        tstringstream file;
        file << path_ << "/record-" << std::put_time(&tm, L"%Y%m%d-%H%M%S") << L".mp4";

        // adapted from http://blog.mmacklin.com/2013/06/11/real-time-video-capture-with-ffmpeg/
        tstringstream cmd;
        cmd << L"ffmpeg -r " << fps_ << " -f rawvideo -pix_fmt rgba "
            << L"-s " << width_ << "x" << height_ << " "
            << L"-i - "
            << L"-threads 2 -y "
            << L"-c:v libx264 -preset ultrafast -qp 0 "
            << make_absolute_path(file.str())
            ;

        tclog << L"Recording video with: " << cmd.str() << std::endl;

#ifdef UNICODE
        ffmpeg_ = _wpopen(cmd.str().c_str(), L"wb");
#else
        ffmpeg_ = _popen(cmd.str().c_str(), "wb");
#endif

        if (!ffmpeg_)
            tcerr << L"Failed to initialize ffmpeg" << std::endl;
    }

    void video_recorder::stop_recording()
    {
        if (ffmpeg_)
            _pclose(ffmpeg_);

        ffmpeg_ = nullptr;
    }

    void video_recorder::add_frame(ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv)
    {
        ID3D11Resource* resource;
        rtv->GetResource(&resource);
        add_frame(context, resource);
        safe_release(resource);
    }

    void video_recorder::add_frame(ID3D11DeviceContext* context, ID3D11Resource* resource)
    {
        context->CopyResource(ffmpeg_texture_, resource);

        D3D11_MAPPED_SUBRESOURCE msr;
        UINT subresource = D3D11CalcSubresource(0, 0, 0);
        context->Map(ffmpeg_texture_, subresource, D3D11_MAP_READ, 0, &msr);

        if (msr.pData && ffmpeg_)
        {
            fwrite(msr.pData, (width_ * height_ * 4), 1, ffmpeg_);
            context->Unmap(ffmpeg_texture_, subresource);
        }
    }

    void video_recorder::destroy()
    {
        stop_recording();
        safe_release(ffmpeg_texture_);
    }

    void dump_rtv(ID3D11Device* device,
        ID3D11DeviceContext* context,
        UINT width, UINT height,
        DXGI_FORMAT format,
        ID3D11RenderTargetView* rtv,
        const dune::tstring& name)
    {
        ID3D11Texture2D* texture;

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Format = format;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;

        if (device->CreateTexture2D(&desc, nullptr, &texture) != S_OK)
        {
            tcerr << L"Failed to create staging texture for recording" << std::endl;
            return;
        }

        ID3D11Resource* resource;
        rtv->GetResource(&resource);

        context->CopyResource(texture, resource);

        D3D11_MAPPED_SUBRESOURCE msr;
        UINT subresource = D3D11CalcSubresource(0, 0, 0);
        context->Map(texture, subresource, D3D11_MAP_READ, 0, &msr);

        if (msr.pData)
        {
            if (format == DXGI_FORMAT_R8G8B8A8_UNORM)
#if 0
                stbi_write_bmp(dune::to_string(dune::make_absolute_path(name + L".bmp")).c_str(), width, height, 4, msr.pData);
#else
                stbi_write_png(dune::to_string(dune::make_absolute_path(name + L".png")).c_str(), width, height, 4, msr.pData, 0);
#endif

            if (format == DXGI_FORMAT_R32G32B32A32_FLOAT)
                stbi_write_hdr(dune::to_string(dune::make_absolute_path(name + L".hdr")).c_str(), width, height, 4, reinterpret_cast<float*>(msr.pData));

            if (format == DXGI_FORMAT_R32G32_FLOAT)
            {
                std::vector<float> data;
                data.resize(width * height * 4);

                float* msrf = reinterpret_cast<float*>(msr.pData);

                for (size_t y = 0; y < height; ++y)
                    for (size_t x = 0; x < width; ++x)
                    {
                        size_t index = (y*width + x);
                        float v = msrf[index * 2 + 0];

                        for (size_t c = 0; c < 3; ++c)
                            data[index * 4 + c] = v;

                        data[index * 4 + 3] = 1.f;
                    }

                stbi_write_hdr(dune::to_string(dune::make_absolute_path(name + L".hdr")).c_str(), width, height, 4, &data[0]);
            }

            context->Unmap(texture, subresource);
        }

        dune::safe_release(resource);
        dune::safe_release(texture);
    }

    void dump_gbuffer(ID3D11Device* device, ID3D11DeviceContext* context, dune::gbuffer& g, const dune::tstring& name)
    {
        for (auto& i : g.targets())
        {
            dump_rtv(device, context, i.desc().Width, i.desc().Height, i.desc().Format, i.rtv(), name + i.name);
        }
    }
}
