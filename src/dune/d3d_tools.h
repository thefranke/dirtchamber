/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_D3D_TOOLS
#define DUNE_D3D_TOOLS

#include <D3D11.h>
#include <DirectXMath.h>

namespace dune
{
    /*!
     * \brief A GPU profiler.
     *
     * This class encapsulates ID3D11Query objects to capture the time spent
     * on completing a portion of a GPU command buffer.
     */
    class profile_query
    {
    protected:
        ID3D11Query* frequency_;
        ID3D11Query* start_;
        ID3D11Query* stop_;

        ID3D11DeviceContext* context_;

    public:
        profile_query();
        virtual ~profile_query() {}

        /*! \brief Create a profile_query object. */
        void create(ID3D11Device* device);

        /*! \brief Destroy a profile_query object and free its resources. */
        void destroy();

        /*! \brief Start a GPU time query. */
        void begin(ID3D11DeviceContext* context);

        /*! \brief Stop measuring time. */
        void end();

        /*! \brief Stop measuring time and return the result in miliseconds. */
        float result();
    };

    template<typename T>
    void set_debug_name(T* obj, const CHAR* name)
    {
        if (obj)
            obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }

    /*! \brief Safely release an object of type T, setting it to nullptr afterwards. */
    template<typename T>
    void safe_release(T& obj)
    {
        if (obj)
        {
            obj->Release();
            obj = nullptr;
        }
    }

    /*!
     * \brief Exchange an object of type T with a new one.
     *
     * Exchange a pointer to an object with a pointer to a new object of the same type if
     * - The old pointer exists
     * - The old pointer isn't the same as the new pointer
     * - The new pointe exists
     *
     * This will safe_release whatever the old object was pointing to and replace it's value by
     * a new pointed to object. This function is useful to replace old shader pointer with new
     * ones after compilation. If the compilation of a new shader fails, the old one is retained.
     *
     * \param oldv A pointer to the old object.
     * \param newv A pointer to the object which should replace oldv.
     */
    template<typename T>
    void exchange(T** oldv, T* newv)
    {
        if (!oldv)
            return;

        if (*oldv == newv)
            return;

        if (newv)
        {
            safe_release(*oldv);
            newv->AddRef();
            *oldv = newv;
        }
    }

    /*!
     * \brief Resize the current viewport.
     *
     * \param context A Direct3D context.
     * \param w The new width.
     * \param h The new height.
     */
    void set_viewport(ID3D11DeviceContext* context, size_t w, size_t h);

    /* \brief Create a new XMFLOAT3 containing only maximum values of a and b. */
    DirectX::XMFLOAT3 max(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);

    /* \brief Create a new XMFLOAT3 containing only minimum values of a and b. */
    DirectX::XMFLOAT3 min(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);

    /* \brief Returns true of a and b contain equal elements. */
    bool equal(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b);

    /*!
     * \brief Clear a list of render targets with a color.
     *
     * \param context A Direct3D context.
     * \param rtvs An array of render target views.
     * \param num_rtvs The number of RTVs in the rtvs array.
     * \param clear_color Four float values specifiying the color with wich to clear all RTVs.
     */
    void clear_rtvs(ID3D11DeviceContext* context,
                    ID3D11RenderTargetView** rtvs,
                    size_t num_rtvs,
                    FLOAT* clear_color);

    /*! \brief Returns true of the SRV is an SRGB view. */
    bool is_srgb(ID3D11ShaderResourceView* rtv);

    /*! \brief Returns true of the DXGI_FORMAT descriptor is SRGB. */
    bool is_srgb(DXGI_FORMAT f);

    void assert_hr_detail(const HRESULT& hr, const char* file, DWORD line, const char* msg);

    /*!
     * \brief An assert for HRESULTs.
     *
     * assert_hr works like a regular assert, except on HRESULT. It will log
     * the filename, the line in the source code and the actual command into cerr and
     * then throw a regular exception with the same error message.
     */
    #define assert_hr(x) assert_hr_detail(x, __FILE__, __LINE__, #x)
}

#endif
