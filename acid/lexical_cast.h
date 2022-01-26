//
// Created by zavier on 2022/1/25.
//

#ifndef ACID_LEXICAL_CAST_H
#define ACID_LEXICAL_CAST_H
#include <type_traits>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <cstring>

namespace acid {
namespace detail {

    template <typename To, typename From>
    struct Converter {
    };

    //to numeric
    template <typename From>
    struct Converter<int, From> {
        static int convert(const From& from) {
            return std::stoi(from);
        }
    };

    template <typename From>
    struct Converter<unsigned int, From> {
        static int convert(const From& from) {
            return std::stoul(from);
        }
    };

    template <typename From>
    struct Converter<long, From> {
        static long convert(const From& from) {
            return std::stol(from);
        }
    };

    template <typename From>
    struct Converter<unsigned long, From> {
        static long convert(const From& from) {
            return std::stoul(from);
        }
    };

    template <typename From>
    struct Converter<long long, From> {
        static long long convert(const From& from) {
            return std::stoll(from);
        }
    };

    template <typename From>
    struct Converter<unsigned long long, From> {
        static long long convert(const From& from) {
            return std::stoull(from);
        }
    };

    template <typename From>
    struct Converter<double, From> {
        static double convert(const From& from) {
            return std::stod(from);
        }
    };

    template <typename From>
    struct Converter<float, From> {
        static float convert(const From& from) {
            return (float)std::stof(from);
        }
    };

    //to bool
    template <typename From>
    struct Converter<bool, From> {
        static typename std::enable_if<std::is_integral<From>::value, bool>::type convert(From from) {
            return !!from;
        }
    };

    bool checkbool(const char* from, const size_t len, const char* s);

    bool convert(const char* from);

    template <>
    struct Converter<bool, std::string> {
        static bool convert(const std::string& from) {
            return detail::convert(from.c_str());
        }
    };

    template <>
    struct Converter<bool, const char*> {
        static bool convert(const char* from) {
            return detail::convert(from);
        }
    };

    template <>
    struct Converter<bool, char*> {
        static bool convert(char* from) {
            return detail::convert(from);
        }
    };

    template <unsigned N>
    struct Converter<bool, const char[N]> {
        static bool convert(const char(&from)[N]) {
            return detail::convert(from);
        }
    };

    template <unsigned N>
    struct Converter<bool, char[N]> {
        static bool convert(const char(&from)[N]) {
            return detail::convert(from);
        }
    };

    //to string
    template <typename From>
    struct Converter<std::string, From> {
        static std::string convert(const From& from) {
            return std::to_string(from);
        }
    };
}

template <typename To, typename From>
typename std::enable_if<!std::is_same<To, From>::value, To>::type
lexical_cast(const From& from) {
    return detail::Converter<To, From>::convert(from);
}

template <typename To, typename From>
typename std::enable_if<std::is_same<To, From>::value, To>::type
lexical_cast(const From& from) {
    return from;
}

}
#endif //ACID_LEXICAL_CAST_H
