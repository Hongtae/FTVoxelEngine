#pragma once
#include "../include.h"
#include <string>
#include <format>

namespace FV
{
    FVCORE_API std::u8string toUTF8(const char*, bool = true);
    FVCORE_API std::u8string toUTF8(const wchar_t*, bool strict = true);
    FVCORE_API std::u8string toUTF8(const char16_t*, bool strict = true);
    FVCORE_API std::u8string toUTF8(const char32_t*, bool strict = true);

    FVCORE_API std::u8string toUTF8(const std::string&, bool = true);
    FVCORE_API std::u8string toUTF8(const std::wstring&, bool strict = true);
    FVCORE_API std::u8string toUTF8(const std::u16string&, bool strict = true);
    FVCORE_API std::u8string toUTF8(const std::u32string&, bool strict = true);

    FVCORE_API std::u16string toUTF16(const std::string&, bool strict = true);
    FVCORE_API std::u16string toUTF16(const std::wstring&, bool strict = true);
    FVCORE_API std::u16string toUTF16(const std::u8string&, bool strict = true);
    FVCORE_API std::u16string toUTF16(const std::u32string&, bool strict = true);
     
    FVCORE_API std::u32string toUTF32(const std::string&, bool strict = true);
    FVCORE_API std::u32string toUTF32(const std::wstring&, bool strict = true);
    FVCORE_API std::u32string toUTF32(const std::u8string&, bool strict = true);
    FVCORE_API std::u32string toUTF32(const std::u16string&, bool strict = true);
}

#ifndef FVCORE_NO_UNICODE_FORMATTER
namespace std
{
    template <> struct formatter<char8_t*> : formatter<const char*>
    {
        auto format(const char8_t* arg, format_context& ctx)
        {
            return formatter<const char*>::format((const char*)arg, ctx);
        }
    };

    template <> struct formatter<u8string> : formatter<char8_t*>
    {
        auto format(const std::u8string& arg, format_context& ctx)
        {
            return formatter<char8_t*>::format(arg.c_str(), ctx);
        }
    };

    template <> struct formatter<u16string> : formatter<u8string>
    {
        auto format(const std::u16string& arg, format_context& ctx)
        {
            return formatter<u8string>::format(FV::toUTF8(arg), ctx);
        }
    };

    template <> struct formatter<char16_t*> : formatter<u16string>
    {
        auto format(const char16_t* arg, format_context& ctxt)
        {
            return formatter<u16string>::format(arg, ctxt);
        }
    };

    template <> struct formatter<u32string> : formatter<u8string>
    {
        auto format(const std::u32string& arg, format_context& ctx)
        {
            return formatter<u8string>::format(FV::toUTF8(arg), ctx);
        }
    };

    template <> struct formatter<char32_t*> : formatter<u32string>
    {
        auto format(const char32_t* arg, format_context& ctxt)
        {
            return formatter<u32string>::format(arg, ctxt);
        }
    };

    template <> struct formatter<const char8_t*> : public formatter<char8_t*> {};
    template <size_t Length> struct formatter<char8_t[Length]> : public formatter<char8_t*> {};
    template <size_t Length> struct formatter<const char8_t[Length]> : public formatter<char8_t*> {};

    template <> struct formatter<const char16_t*> : public formatter<char16_t*> {};
    template <size_t Length> struct formatter<char16_t[Length]> : public formatter<char16_t*> {};
    template <size_t Length> struct formatter<const char16_t[Length]> : public formatter<char16_t*> {};

    template <> struct formatter<const char32_t*> : public formatter<char32_t*> {};
    template <size_t Length> struct formatter<char32_t[Length]> : public formatter<char32_t*> {};
    template <size_t Length> struct formatter<const char32_t[Length]> : public formatter<char32_t*> {};
}
#endif //#ifndef FVCORE_NO_UNICODE_FORMATTER
