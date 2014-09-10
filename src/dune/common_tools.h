/* 
 * dune::common_tools by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_COMMON_TOOLS
#define DUNE_COMMON_TOOLS

#include "unicode.h"
#include <vector>

namespace dune 
{
    /*! \brief Extract a path from a given pattern, e.g. "C:/foo" from "C:/foo/\*.bar". */
    tstring extract_path(const tstring& pattern);

    /*! \brief Returns true if the supplied path p was a relative URI. */
    bool path_is_relative(const tstring& p);

    /*! \brief Returns an absolute path of the current execution directory. */
    tstring absolute_path();
    
    /*! \brief Return an absolute URI to a URI relative to the execution directory. Thus function is used in every loader. */
    tstring make_absolute_path(const tstring& relative_filename);

    /*! \brief Return a vector of strings from a given argument string. A default file will be pushed to the vector if args was empty. */
    std::vector<tstring> files_from_args(const tstring& args, const tstring& default_file = L"");
}

#endif