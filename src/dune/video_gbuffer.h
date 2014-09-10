/* 
 * dune::video_gbuffer by Tobias Alexander Franke (tob@cyberhead.de) 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_VIDEO_GBUFFER
#define DUNE_VIDEO_GBUFFER

#include "gbuffer.h"

#include <DShow.h>

interface ISampleGrabberCB : public IUnknown
{
	virtual STDMETHODIMP SampleCB( double SampleTime, IMediaSample *pSample ) = 0;
	virtual STDMETHODIMP BufferCB( double SampleTime, BYTE *pBuffer, long BufferLen ) = 0;
};

interface ISampleGrabber : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE SetOneShot( BOOL OneShot ) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetMediaType( const AM_MEDIA_TYPE *pType ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType( AM_MEDIA_TYPE *pType ) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetBufferSamples( BOOL BufferThem ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer( long *pBufferSize, long *pBuffer ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCurrentSample( IMediaSample **ppSample ) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetCallback( ISampleGrabberCB *pCallback, long WhichMethodToCallback ) = 0;
};

static const IID IID_ISampleGrabber     = { 0x6B652FFF, 0x11FE, 0x4fce, { 0x92, 0xAD, 0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F } };
static const IID IID_ISampleGrabberCB   = { 0x0579154A, 0x2B53, 0x4994, { 0xB0, 0xD0, 0xE7, 0x73, 0x14, 0x8E, 0xFF, 0x85 } };
static const CLSID CLSID_SampleGrabber  = { 0xC1F400A0, 0x3F08, 0x11d3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };
static const CLSID CLSID_NullRenderer   = { 0xC1F400A4, 0x3F08, 0x11d3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };

#define RT_VIDEO_COLOR L"color"

namespace dune 
{
    class video_gbuffer : public gbuffer
    {
    protected:
        struct grabber_callback : public ISampleGrabberCB
	    {
            video_gbuffer* caller;
		    virtual HRESULT __stdcall SampleCB(double time, IMediaSample* sample);
		    virtual HRESULT __stdcall BufferCB(double time, BYTE* buffer, long len);
		    virtual HRESULT __stdcall QueryInterface( REFIID iid, LPVOID *ppv );
		    virtual ULONG	__stdcall AddRef();
		    virtual ULONG	__stdcall Release();
	    };

        friend struct grabber_callback;

	    CRITICAL_SECTION        cs_;
	    bool                    async_;
        ID3D11DeviceContext*    context_;
        bool                    running_;
        std::vector<BYTE>       temp_buffer_;

        IFilterGraph2*			graph_;
	    ICaptureGraphBuilder2*	capture_;
	    IMediaControl*			control_;

	    IBaseFilter*			nullf_;
	    IBaseFilter*			sgf_;
        IBaseFilter*            current_device_;
	    ISampleGrabber*			sg_;

        grabber_callback        callback_;

        typedef void (*video_cb)(BYTE* buffer, size_t width, size_t height);
        video_cb                external_callback_;

        bool                    has_new_buffer_;

    protected:
	    HRESULT init_device(UINT32 index, size_t w, size_t h, bool async);

    public:
        video_gbuffer();
        virtual ~video_gbuffer() {}

	    virtual void create(ID3D11Device* device, UINT32 device_index, const tstring& name, UINT w = 640, UINT h = 480);
        void set_callback(video_cb cb);
	
        void destroy();

	    HRESULT start(ID3D11DeviceContext* context);
	    void stop();

	    void update();
    };
}

#endif