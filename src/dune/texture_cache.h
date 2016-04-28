/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_TEXTURE_CACHE
#define DUNE_TEXTURE_CACHE

#include <map>

#include <D3D11.h>
#include <boost/noncopyable.hpp>

#include "unicode.h"
#include "texture.h"

namespace dune
{
    /*!
     * \brief A texture cache.
     *
     * The texture_cache is a single instance which keeps track of all currently loaded texture resources.
     * By requesting texture objects from this class, multiple requests of the same file will be cached
     * instead of creating additional textures in memory.
     */
    class texture_cache : boost::noncopyable
    {
    protected:
        static std::map<tstring, ID3D11ShaderResourceView*> texture_cache_;

        texture_cache() {};

    public:
        /*! \brief The static instance of the texture_cache. */
        static texture_cache& i();

        /*!
         * \brief Add a new texture to the cache.
         *
         * Load a new texture and add it to the cache. If the texture has already been loaded, nothing happens.
         *
         * \param device The Direct3D device.
         * \param filename A string of the filename on the disk.
         */
        void add_texture(ID3D11Device* device, const tstring& filename);

        /*
         * \ brief Request a shader resource view for a texture.
         *
         * Request a shader resource view for a texture identified by a filename. If the texture hasn't been loaded yet,
         * it will be automatically added to the cache. Otherwise, the already cached texture's SRV will be returned.
         *
         * \param filename A string of the filename on the disk.
         * \return An SRV.
         */
        ID3D11ShaderResourceView* srv(const tstring& filename) const;

        void generate_mips(ID3D11DeviceContext* context);

        /*! \brief Destroy the texture_cache and free all resources. */
        void destroy();
    };

    /*!
     * \brief The principal function to load a texture.
     *
     * This function is used to load textures of any type. The correct loader will be deduced from the filename. Optionally,
     * a shader resource view can be directly retreived for the file. This function will automatically populate the texture_cache
     * instance.
     *
     * \param device The Direct3D device
     * \param filename A string of the filename on the disk
     * \param srv An optional pointer to an empty SRV pointer. If this is nullptr, the parameter is ignored.
     */
    void load_texture(ID3D11Device* device, const tstring& filename, ID3D11ShaderResourceView** srv = nullptr);

    void load_texture(ID3D11Device* device, const tstring& filename, texture& t);
}

#endif
