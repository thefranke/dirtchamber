/*
 * Dune D3D library - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "logger.h"

#include "common_tools.h"

#include <Windows.h>

#include <boost/shared_array.hpp>

namespace dune
{
    namespace logger
    {
        enum mtype
        {
            message,
            warning,
            error,
        };

        static tofstream log_file;
        static tstringstream warnings;

        void write(const tstring& text, mtype type);
        void display(UINT type);

        template<class charT, class traits = std::char_traits<charT> >
        class logbuf : public std::basic_streambuf<charT, traits>
        {
        public:
            typedef charT                          char_type;
            typedef traits                         traits_type;
            typedef typename traits_type::int_type int_type;

            mtype type_;
            const size_t block_size_;
            boost::shared_array<charT> buffer_;
            tstringstream mbuf_;

            logbuf(mtype t) : type_(t), block_size_ (16), mbuf_()
            {
                buffer_.reset(new charT[block_size_]);
                reset();
            }

        protected:
            void reset()
            {
                charT* inp = buffer_.get();
                this->setp(inp, inp + block_size_);
                std::memset(inp, 0, block_size_);
            }

            virtual int_type overflow(int_type c = traits_type::eof())
            {
                if (this->pptr() != this->epptr())
                    return traits_type::not_eof(*this->pptr());

                sync();

                return this->sputc(traits_type::to_char_type(c));
            }

            virtual int sync()
            {
                size_t size = this->pptr() - this->pbase();

                tstring s;
                if (size > 0)
                {
                    s.assign(this->pbase(), size);

                    // collect data until there is no more
                    mbuf_ << s;
                }

                // no more incoming data -> flush out
                if (size > 0 && size < block_size_
                     && (*s.rbegin() == L'\0' || *s.rbegin() == L'\n'))
                {
                    write(mbuf_.str(), type_);
                    mbuf_.str(L"");
                }

                reset();

                return 0;
            }
        };

        void init(const tstring& filename)
        {
            static logbuf<TCHAR> buf_warnings(warning);
            static logbuf<TCHAR> buf_errors(error);
            static logbuf<TCHAR> buf_messages(message);

            warnings.str(L"");

            tcout.rdbuf(&buf_warnings);
            tcerr.rdbuf(&buf_errors);
            tclog.rdbuf(&buf_messages);

            if (filename != L"")
                log_file.open(make_absolute_path(filename).c_str());
        }

        void display(tstring m, UINT type)
        {
            if (m == L"\n" || m == L"")
                return;

            const TCHAR* title = type == MB_ICONINFORMATION ? L"Warnings" : L"Errors";
            MessageBox(0, m.c_str(), title, type);
        }

        void show_warnings()
        {
            tstring w = warnings.str();

            if (!w.empty())
            {
                display(warnings.str(), MB_ICONINFORMATION);
                warnings.str(L"");
            }
        }

        void write(const tstring& text, mtype type)
        {
            if (type == warning)
                warnings << text;

            if (type == error)
                display(text, MB_ICONERROR);

            if (log_file.good())
            {
                if (type == warning) log_file << std::endl << L"Warning: " << std::endl;
                if (type == error) log_file << std::endl << L"ERROR: " << std::endl;
                log_file << text;
                if (type != message) log_file << std::endl;
                log_file.flush();
            }
        }
    }
}
