/* 
 * dune::unicode by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_UNICODE
#define DUNE_UNICODE

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

namespace dune 
{ 
    #ifdef UNICODE

    typedef std::wstring tstring;
    typedef std::wifstream tifstream;
    typedef std::wofstream tofstream;
    typedef std::wstringstream tstringstream;
    #define tcerr std::wcerr
    #define tclog std::wclog   
    #define tcout std::wcout

    tstring to_tstring(const std::string& str);
    std::string to_string(const tstring& str);

    #else

    typedef std::string tstring;
    typedef std::ifstream tifstream;
    typedef std::ofstream tofstream;
    typedef std::stringstream tstringstream;
    #define tcerr std::cerr
    #define tclog std::clog
    #define tcout std::cout

    tstring to_tstring(const std::wstring& str);
    std::wstring to_string(const tstring& str);

    #endif
}

#endif