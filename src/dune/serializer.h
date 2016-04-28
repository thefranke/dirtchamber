/*
 * Dune D3D library - Tobias Alexander Franke 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_SERIALIZER
#define DUNE_SERIALIZER

#include "exception.h"

#include <boost/any.hpp>
#include <map>

#include "unicode.h"

namespace dune
{
    typedef int BOOL;

    /*!
     * \brief Seralizer to read/write Dune objects from/into JSON/XML.
     *
     * This class manages a map of key-value pairs of tstring objects and can
     * read or write them to XML files. Keys can be structured into subkeys by
     * dividing them with dots. For instace, the hierarchy
     *
     * > foo { bar { baz }, bar1 { baz } }
     *
     * can be expressed with the following keys
     *
     * > foo.bar.baz
     *
     * > foo.bar1.baz
     *
     * The serializer is used to save information about Dune objects into
     * files. This enables loading program configurations.
     */
    class serializer
    {
    protected:
        std::map<tstring, tstring> properties_;

    public:
        /*! \brief Save the current key-value's into a file specified by filename. */
        void save(const tstring& filename);

        /*! \brief Load key-value pairs from a file specified by filename. */
        void load(const tstring& filename);

        /*!
         * \brief Get a value from a specified key.
         *
         * Fetch a value from the storage and return it as type T.
         *
         * \tparam T The type of the parameter.
         * \param key The key specifying the value.
         * \return The requested value.
         * \throws exception The key does not exist.
         */
        template<typename T>
        T get(const tstring& key) const
        {
            auto i = properties_.find(key);

            if (i == properties_.end())
                throw dune::exception(L"Unknown key: " + key);

            T value;
            tstringstream ss(i->second);

            ss >> std::boolalpha >> value;
            return value;
        }

        /*!
         * \brief Add a new key-value pair or overwrite an existing key with a new value.
         *
         * Add a value of type T to the storage. If the key already exists, the value
         * is simply replaced.
         *
         * \tparam T The type of the parameter.
         * \param key The key specifying the value.
         * \param value The value.
         */
        template<typename T>
        void put(const tstring& key, const T& value)
        {
            tstringstream ss;
            ss << value;
            properties_[key] = ss.str();
        }

        // Special case for type BOOL
        template<>
        void put(const tstring& key, const BOOL& value)
        {
            tstringstream ss;

            // BOOL is actually int, meh
            if (value == 0)
                ss << std::boolalpha << false;
            else if (value == 1)
                ss << std::boolalpha << true;
            else
                ss << value;

            properties_[key] = ss.str();
        }
    };
}

#endif
