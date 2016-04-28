/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_LOGGER
#define DUNE_LOGGER

#include "unicode.h"

namespace dune
{
    namespace logger
    {
        /*! \brief Initialize the logging mechanism for dune. All cout/clog/cerr commands will reroute their output into a file identified by filename. */
        void init(const tstring& filename);

        /*! \brief Show all warnings up to the call of this function collected by the logger with a popup. */
        void show_warnings();
    };
}

#endif
