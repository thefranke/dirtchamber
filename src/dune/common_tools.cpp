/* 
 * dune::common_tools by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common_tools.h"

#include <algorithm>
#include <iostream>
#include <codecvt>
#include <locale>

#include <Windows.h>

namespace dune 
{
    std::vector<tstring> files_from_args(const tstring& arg, const tstring& default_file)
    {
        std::vector<tstring> files;
        
        if (arg != L"")
        {
            int argsc;
            LPWSTR *args = CommandLineToArgvW(arg.c_str(), &argsc);

            for (int i = 0; i < argsc; ++i)
                files.push_back(dune::make_absolute_path(args[i]));
        }

        if (files.empty() && default_file != L"")
            files.push_back(dune::make_absolute_path(default_file));

        return files;
    }

    tstring extract_path(const tstring& pattern)
    {
        tstring path = pattern;

        std::replace(path.begin(), path.end(), L'\\', L'/');
        tstring::size_type pos = path.find_last_of(L"/");

        return path.substr(0, pos+1);
    }

    bool path_is_relative(const tstring& p)
    {
	    return (p.find(L":\\") == tstring::npos) &&
		       (p.find(L":/")  == tstring::npos);
    }

    tstring absolute_path()
    {
	    // get executbale path
	    const size_t s = 512;
	    TCHAR path[s];
	    if (GetModuleFileName(nullptr, path, s) == ERROR_INSUFFICIENT_BUFFER)
	    {
		    tcerr << L"Failed to determine execution path" << std::endl;
		    return L"";
	    }

	    return extract_path(path);
    }

    tstring make_absolute_path(const tstring& relative_filename)
    {
	    if (!path_is_relative(relative_filename))
		    return relative_filename;

	    tstring full_filename = absolute_path();
	    full_filename += relative_filename;

	    return full_filename;
    }

    std::wstring ansi_to_wide(const std::string& str)
    {
        std::wstring str2(str.length(), L' ');
        std::copy(str.begin(), str.end(), str2.begin());
        return str2;
    }

    std::string wide_to_ansi(const std::wstring& str)
    {
	    static std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
        return utf8_conv.to_bytes(str);
    }

    #ifdef UNICODE

    tstring to_tstring(const std::string& str)
    {
	    return ansi_to_wide(str);
    }

    std::string to_string(const tstring& str)
    {
	    return wide_to_ansi(str);
    }

    #else

    tstring to_tstring(const std::wstring& str)
    {
	    return wide_to_ansi(str);
    }

    std::wstring to_string(const tstring& str)
    {
	    return ansi_to_wide(str);
    }

    #endif
}
