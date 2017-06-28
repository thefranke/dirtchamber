/*
 * Dune D3D library - Tobias Alexander Franke 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_SERIALIZER_TOOLS
#define DUNE_SERIALIZER_TOOLS

#include "serializer.h"

namespace dune
{
    class camera;
    class directional_light;
    class light_propagation_volume;
    class sparse_voxel_octree;
}

namespace dune
{
    //!@{
    /*! \brief Read/write camera parameters from/to a serializer. */
    serializer& operator<<(serializer& s, const camera& c);
    const serializer& operator>>(const serializer& s, camera& c);
    //!@}

    //!@{
    /*! \brief Read/write directional_light parameters from/to a serializer. */
    serializer& operator<<(serializer& s, const directional_light& l);
    const serializer& operator>>(const serializer& s, directional_light& l);
    //!@}

    //!@{
    /*! \brief Read/write light_propagation_volume parameters from/to a serializer. */
    serializer& operator<<(serializer& s, const light_propagation_volume& lpv);
    const serializer& operator>>(const serializer& s, light_propagation_volume& lpv);
    //!@}

    //!@{
    /*! \brief Read/write sparse_voxel_octree parameters from/to a serializer. */
    serializer& operator<<(serializer& s, const sparse_voxel_octree& lpv);
    const serializer& operator>>(const serializer& s, sparse_voxel_octree& lpv);
    //!@}
}

#endif
