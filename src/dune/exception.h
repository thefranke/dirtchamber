/*
* Dune D3D library - Tobias Alexander Franke 2011
* For copyright and license see LICENSE
* http://www.tobias-franke.eu
*/

/*! \file */

#ifndef DUNE_EXCEPTION
#define DUNE_EXCEPTION

#include <stdexcept>

#include "unicode.h"

namespace dune
{
    /*!
     * \brief Exception class.
     *
     * A wrapper class for std::exception which adds support
     * for multibyte character messages. Call msg() instead of what()
     * to get a tstring of the exception.
     */
    class exception : public std::exception
    {
    protected:
        tstring msg_;

    public:
        exception(tstring msg)
        {
            msg_ = msg;
        }

        virtual const char* what() const
        {
            return to_string(msg_).c_str();
        }

        /*! \brief Return exception message as tstring. */
        virtual const tstring& msg() const
        {
            return msg_;
        }

        virtual ~exception() throw()
        {
        }
    };
}

#endif
