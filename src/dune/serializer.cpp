/*
 * dune::serializer by Tobias Alexander Franke (tob@cyberhead.de) 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "serializer.h"

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/json_parser.hpp>

#ifdef UNICODE
typedef boost::property_tree::wptree tptree;
#else
typedef boost::property_tree::ptree tptree;
#endif

typedef boost::property_tree::xml_writer_settings<dune::tstring> txml_writer_settings;

namespace dune
{
    namespace detail
    {
        void to_properties(tstring path, const tptree& pt, std::map<tstring, tstring>& properties)
        {
            for (const auto& i : pt)
            {
                tstring subpath = path;

                if (subpath != L"")
                    subpath += L".";

                subpath += i.first;

                if (!i.second.empty())
                {
                    to_properties(subpath, i.second, properties);
                }
                else
                    properties[subpath] = i.second.data();
            }
        }

        void from_properties(const std::map<tstring, tstring>& properties, tptree& pt)
        {
            for (const auto& i : properties)
                pt.add(i.first, i.second);
        }
    }

    void serializer::load(const tstring& filename)
    {
        tptree pt;
        boost::property_tree::read_xml(dune::to_string(filename), pt);

        detail::to_properties(L"", pt, properties_);
    }

    void serializer::save(const tstring& filename)
    {
        tptree pt;
        detail::from_properties(properties_, pt);

        txml_writer_settings settings(L'\t', 1);
        boost::property_tree::write_xml(dune::to_string(filename), pt, std::locale(), settings);
    }
}
