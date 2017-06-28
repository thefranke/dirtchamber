/*
 * Dune D3D library - Tobias Alexander Franke 2017
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_RECORD_TOOLS
#define DUNE_RECORD_TOOLS

#include "unicode.h"

#include <D3D11.h>

namespace dune
{
    class gbuffer;
}

namespace dune
{
    class video_recorder
    {
    protected:
        UINT                    width_,
                                height_,
                                fps_;
        ID3D11Texture2D*        ffmpeg_texture_;
        FILE*                   ffmpeg_;
        tstring                 path_;

    public:
        video_recorder();
        virtual ~video_recorder();

        void create(ID3D11Device* device, UINT width, UINT height, UINT fps, const tstring& path = L"../../data");
        void destroy();

        void start_recording();
        void stop_recording();
        void add_frame(ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv);
        void add_frame(ID3D11DeviceContext* context, ID3D11Resource* resource);
    };

    void dump_rtv(ID3D11Device* device, ID3D11DeviceContext* context, UINT width, UINT height, DXGI_FORMAT format, ID3D11RenderTargetView* rtv, const tstring& name);
    void dump_gbuffer(ID3D11Device* device, ID3D11DeviceContext* context, gbuffer& g, const tstring& name);
}

#endif
