#include "Unicode.h"
#include <vector>

namespace {
    static const uint8_t trailingBytesForUTF8[256] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
    };
    static const char32_t offsetsFromUTF8[6] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL, 0x03C82080UL, 0xFA082080UL, 0x82082080UL };
    static const char8_t firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

    bool isLegalUTF8(const char8_t* str, size_t len) {
        char8_t ch;
        const char8_t* p = &str[len];
        switch (len) {
        default:
            return false;
        case 4:	if ((ch = (*--p)) < 0x80 || ch > 0xbf)	return false;
        case 3: if ((ch = (*--p)) < 0x80 || ch > 0xbf)	return false;
        case 2: if ((ch = (*--p)) > 0xbf) return false;
            switch (*str & 0xff) {
            case 0xe0: if (ch < 0xa0) return false; break;
            case 0xed: if (ch > 0x9f) return false; break;
            case 0xf0: if (ch < 0x90) return false; break;
            case 0xf4: if (ch < 0x80) return false;
            }
        case 1: if (static_cast<char8_t>(*str & 0xff) >= 0x80 && static_cast<char8_t>(*str & 0xff) < 0xc2) return false;
        }
        if ((*str & 0xff) > 0xf4) return false;
        return true;
    }

    bool isLegalUTF8String(const char8_t* input, size_t inputLen) {
        const char8_t* inputBegin = reinterpret_cast<const char8_t*>(input);
        const char8_t* inputEnd = reinterpret_cast<const char8_t*>(&input[inputLen]);
        while (inputBegin != inputEnd) {
            ptrdiff_t length = trailingBytesForUTF8[(*inputBegin) & 0xff] + 1;
            if (length > (inputEnd - inputBegin) || !isLegalUTF8(inputBegin, length))
                return false;
            inputBegin += length;
        }
        return true;
    }

    constexpr auto unicodeHighSurrogateBegin = 0xD800;
    constexpr auto unicodeHighSurrogateEnd = 0xDBFF;
    constexpr auto unicodeLowSurrogateBegin = 0xDC00;
    constexpr auto unicodeLowSurrogateEnd = 0xDFFF;

    constexpr auto unicodeReplacementChar = 0x0000FFFD;
    constexpr auto unicodeMaxUTF16 = 0x0010FFFF;
    constexpr auto unicodeMaxUTF32 = 0x7FFFFFFF;
    constexpr auto unicodeMaxLegalUTF32 = 0x0010FFFF;
    constexpr auto unicodeHalfBase = 0x10000;
    constexpr auto unicodeHalfMask = 0x3FFU;
    constexpr auto unicodeHalfShift = 10;

    template <typename UTF16Setter>
    bool convertUTF8toUTF16(const char8_t* input, const char8_t* inputEnd, bool strict, UTF16Setter&& setter) {
        while (input < inputEnd) {
            char32_t ch = 0;
            unsigned short extraBytesToRead = trailingBytesForUTF8[*input & 0xff];
            if (extraBytesToRead >= inputEnd - input)
                return false;
            if (!isLegalUTF8(input, extraBytesToRead + 1))
                return false;

            switch (extraBytesToRead) {
            case 5: ch += (*input++) & 0xff; ch <<= 6;		// illegal utf8
            case 4: ch += (*input++) & 0xff; ch <<= 6;		// illegal utf8
            case 3: ch += (*input++) & 0xff; ch <<= 6;
            case 2: ch += (*input++) & 0xff; ch <<= 6;
            case 1: ch += (*input++) & 0xff; ch <<= 6;
            case 0: ch += (*input++) & 0xff;
            }
            ch -= offsetsFromUTF8[extraBytesToRead];
            if (ch <= 0xFFFF) {
                // surrogate pair is not valid on UTF-32
                if (ch >= unicodeHighSurrogateBegin && ch <= unicodeLowSurrogateEnd) {
                    if (strict)
                        return false;
                    else
                        setter((char16_t)unicodeReplacementChar);
                } else
                    setter((char16_t)ch); // valid character.
            } else if (ch > unicodeMaxUTF16) // out of range.
            {
                if (strict)
                    return false;
                else
                    setter((char16_t)unicodeReplacementChar);
            } else // 0xFFFF ~ 0x10FFFF is valid range.
            {
                ch -= unicodeHalfBase;
                setter((char16_t)((ch >> unicodeHalfShift) + unicodeHighSurrogateBegin));
                setter((char16_t)((ch & unicodeHalfMask) + unicodeLowSurrogateBegin));
            }
        }
        return true;
    }

    template <typename UTF32Setter>
    bool convertUTF8toUTF32(const char8_t* input, const char8_t* inputEnd, bool strict, UTF32Setter&& setter) {
        while (input < inputEnd) {
            unsigned short extraBytesToRead = trailingBytesForUTF8[*input & 0xff];
            if (extraBytesToRead >= inputEnd - input)
                return false;
            if (!isLegalUTF8(input, extraBytesToRead + 1))
                return false;

            char32_t ch = 0;
            switch (extraBytesToRead) {
            case 5: ch += (*input++) & 0xff; ch <<= 6;
            case 4: ch += (*input++) & 0xff; ch <<= 6;
            case 3: ch += (*input++) & 0xff; ch <<= 6;
            case 2: ch += (*input++) & 0xff; ch <<= 6;
            case 1: ch += (*input++) & 0xff; ch <<= 6;
            case 0: ch += (*input++) & 0xff;
            }
            ch -= offsetsFromUTF8[extraBytesToRead];

            if (ch <= unicodeMaxLegalUTF32) {
                // UTF-16 surrogate is not valid on UTF-32
                // not valid character which bigger than 0x10FFFF
                if (ch >= unicodeHighSurrogateBegin && ch <= unicodeLowSurrogateEnd) {
                    if (strict)
                        return false;
                    else
                        setter((char32_t)unicodeReplacementChar);
                } else
                    setter(ch);
            } else // out of range.
            {
                if (strict)
                    return false;
                else
                    setter((char32_t)unicodeReplacementChar);
            }
        }
        return true;
    }

    template <typename UTF8Setter>
    bool convertUTF16toUTF8(const char16_t* input, const char16_t* inputEnd, bool strict, UTF8Setter&& setter) {
        while (input < inputEnd) {
            char32_t ch = (*input++) & 0xffff;
            if (ch >= unicodeHighSurrogateBegin && ch <= unicodeHighSurrogateEnd) {
                // UTF-16 surrogate pair, convert UTF-32 first.
                if (input < inputEnd) {
                    char32_t ch2 = (*input) & 0xffff;
                    if (ch2 >= unicodeLowSurrogateBegin && ch2 <= unicodeLowSurrogateEnd) {
                        // low-surrogate, convert UTF-32
                        ch = ((ch - unicodeHighSurrogateBegin) << unicodeHalfShift) + (ch2 - unicodeLowSurrogateBegin) + unicodeHalfBase;
                        ++input;
                    } else if (strict) // high-surrogate, mismatched pair.
                        return false;
                } else // string is too short.
                {
                    return false;
                }
            } else if (strict) {
                // UTF-16 surrogate is not valid on UTF-32.
                if (ch >= unicodeLowSurrogateBegin && ch <= unicodeLowSurrogateEnd)
                    return false;
            }

            // calculate result.
            unsigned short bytesToWrite = 0;
            if (ch < 0x80)                      bytesToWrite = 1;
            else if (ch < 0x800)                bytesToWrite = 2;
            else if (ch < 0x10000)              bytesToWrite = 3;
            else if (ch < 0x110000)             bytesToWrite = 4;
            else { ch = unicodeReplacementChar;	bytesToWrite = 3; }

            char8_t data[4];
            switch (bytesToWrite) {
            case 4:	data[3] = (char8_t)((ch | 0x80) & 0xBF); ch >>= 6;
            case 3: data[2] = (char8_t)((ch | 0x80) & 0xBF); ch >>= 6;
            case 2: data[1] = (char8_t)((ch | 0x80) & 0xBF); ch >>= 6;
            case 1: data[0] = (char8_t)(ch | firstByteMark[bytesToWrite]);
                for (unsigned int i = 0; i < bytesToWrite; ++i)
                    setter(data[i]);
            }
        }
        return true;
    }

    template <typename UTF32Setter>
    bool convertUTF16toUTF32(const char16_t* input, const char16_t* inputEnd, bool strict, UTF32Setter&& setter) {
        while (input < inputEnd) {
            char32_t ch = (*input++) & 0xffff;
            if (ch >= unicodeHighSurrogateBegin && ch <= unicodeHighSurrogateEnd) {
                // convert surrogate pair to UTF-32.
                if (input < inputEnd) {
                    char32_t ch2 = (*input) & 0xffff;
                    // convert UTF-32, if value is in low-surrogate range.
                    if (ch2 >= unicodeLowSurrogateBegin && ch2 <= unicodeLowSurrogateEnd) {
                        ch = ((ch - unicodeHighSurrogateBegin) << unicodeHalfShift) + (ch2 - unicodeLowSurrogateBegin) + unicodeHalfBase;
                        ++input;
                    } else if (strict) // no value for high-surrogate.
                        return false;
                }
            } else if (strict) {
                if (ch >= unicodeLowSurrogateBegin && ch <= unicodeLowSurrogateEnd)	// UTF-32 cannot have surrogate pair
                    return false;
            }
            setter(ch);
        }
        return true;
    }

    template <typename UTF8Setter>
    bool convertUTF32toUTF8(const char32_t* input, const char32_t* inputEnd, bool strict, UTF8Setter&& setter) {
        while (input < inputEnd) {
            char32_t ch = *input++;
            if (strict) {
                if (ch >= unicodeHighSurrogateBegin && ch <= unicodeLowSurrogateEnd)	// UTF-16 surrogate pair is not valid on UTF-32
                    return false;
            }

            // calculate result.
            unsigned short bytesToWrite = 0;
            if (ch < 0x80)						bytesToWrite = 1;
            else if (ch < 0x800)				bytesToWrite = 2;
            else if (ch < 0x10000)				bytesToWrite = 3;
            else if (ch <= unicodeMaxLegalUTF32)bytesToWrite = 4;
            else { ch = unicodeReplacementChar; bytesToWrite = 3; }

            char8_t data[4];
            switch (bytesToWrite) {
            case 4:	data[3] = (char8_t)((ch | 0x80) & 0xBF); ch >>= 6;
            case 3: data[2] = (char8_t)((ch | 0x80) & 0xBF); ch >>= 6;
            case 2: data[1] = (char8_t)((ch | 0x80) & 0xBF); ch >>= 6;
            case 1: data[0] = (char8_t)(ch | firstByteMark[bytesToWrite]);
                for (unsigned int i = 0; i < bytesToWrite; ++i)
                    setter(data[i]);
            }
        }
        return true;
    }

    template <typename UTF16Setter>
    bool convertUTF32toUTF16(const char32_t* input, const char32_t* inputEnd, bool strict, UTF16Setter&& setter) {
        while (input < inputEnd) {
            char32_t ch = *input++;
            if (ch <= 0xFFFF) {
                if (ch >= unicodeHighSurrogateBegin && ch <= unicodeLowSurrogateEnd) {
                    if (strict)
                        return false;
                    else
                        setter((char16_t)unicodeReplacementChar);
                } else
                    setter((char16_t)ch); // valid character.
            } else if (ch > unicodeMaxLegalUTF32) {
                if (strict)
                    return false;
                else
                    setter((char16_t)unicodeReplacementChar);
            } else // value is in range of 0xFFFF ~ 0x10FFFF
            {
                ch -= unicodeHalfBase;
                setter((char16_t)((ch >> unicodeHalfShift) + unicodeHighSurrogateBegin));
                setter((char16_t)((ch & unicodeHalfMask) + unicodeLowSurrogateBegin));
            }
        }
        return true;
    }
}

namespace FV {
    template <typename T, size_t S = sizeof(T)> struct _WNativeString;
    template <> struct _WNativeString<wchar_t, 4> {
        static std::u32string get(const std::wstring& input) {
            return { reinterpret_cast<const char32_t*>(input.c_str()) };
        }
    };
    template <> struct _WNativeString<wchar_t, 2> {
        static std::u16string get(const std::wstring& input) {
            return { reinterpret_cast<const char16_t*>(input.c_str()) };
        }
    };
    using WNativeString = _WNativeString<wchar_t>;

    FVCORE_API std::string string(const std::string& input, bool strict) {
        return { input.c_str() };
    }

    FVCORE_API std::string string(const std::wstring& input, bool strict) {
        return string(WNativeString::get(input), strict);
    }

    FVCORE_API std::string string(const std::u8string& input, bool strict) {
        return { reinterpret_cast<const char*>(input.c_str()) };
    }

    FVCORE_API std::string string(const std::u16string& input, bool strict) {
        auto length = input.size();
        std::vector<char8_t> output;
        output.reserve(length * 2);

        if (convertUTF16toUTF8(input.data(),
                               input.data() + length,
                               strict,
                               [&](char8_t ch) {
                                   output.push_back(ch);
                               }) == false) {
            return {};
        }
        return { output.begin(), output.end() };
    }

    FVCORE_API std::string string(const std::u32string& input, bool strict) {
        auto length = input.size();
        std::vector<char8_t> output;
        output.reserve(length * 4);

        if (convertUTF32toUTF8(input.data(),
                               input.data() + length,
                               strict,
                               [&](char8_t ch) {
                                   output.push_back(ch);
                               }) == false) {
            return {};
        }
        return { output.begin(), output.end() };
    }

    FVCORE_API std::wstring wstring(const std::string& input, bool strict) {
        return wstring(std::u8string(input.begin(), input.end()), strict);
    }

    FVCORE_API std::wstring wstring(const std::wstring& input, bool strict) {
        return { input.c_str() };
    }

    FVCORE_API std::wstring wstring(const std::u8string& input, bool strict) {
        switch (sizeof(wchar_t)) {
        case (sizeof(char16_t)):
            do {
                auto s = u16string(input, strict);
                return { reinterpret_cast<const wchar_t*>(s.c_str()) };
            } while (0);
            break;
        case (sizeof(char32_t)):
            do {
                auto s = u32string(input, strict);
                return { reinterpret_cast<const wchar_t*>(s.c_str()) };
            } while (0);
            break;
        }
        return {};
    }

    FVCORE_API std::wstring wstring(const std::u16string& input, bool strict) {
        switch (sizeof(wchar_t)) {
        case (sizeof(char16_t)):
            return { reinterpret_cast<const wchar_t*>(input.c_str()) };
        case (sizeof(char32_t)):
            do {
                auto s = u32string(input, strict);
                return { reinterpret_cast<const wchar_t*>(s.c_str()) };
            } while (0);
            break;
        }
        return {};
    }

    FVCORE_API std::wstring wstring(const std::u32string& input, bool strict) {
        switch (sizeof(wchar_t)) {
        case (sizeof(char16_t)):
            do {
                auto s = u16string(input, strict);
                return { reinterpret_cast<const wchar_t*>(s.c_str()) };
            } while (0);
            break;
        case (sizeof(char32_t)):
            return { reinterpret_cast<const wchar_t*>(input.c_str()) };
        }
        return {};
    }

    FVCORE_API std::u8string u8string(const std::string& input, bool strict) {
        return { reinterpret_cast<const char8_t*>(input.c_str()) };
    }

    FVCORE_API std::u8string u8string(const std::wstring& input, bool strict) {
        return u8string(WNativeString::get(input), strict);
    }

    FVCORE_API std::u8string u8string(const std::u8string& input, bool strict) {
        return { input.c_str() };
    }

    FVCORE_API std::u8string u8string(const std::u16string& input, bool strict) {
        auto length = input.size();
        std::vector<char8_t> output;
        output.reserve(length * 2);

        if (convertUTF16toUTF8(input.data(),
                               input.data() + length,
                               strict,
                               [&](char8_t ch) {
                                   output.push_back(ch);
                               }) == false) {
            return {};
        }
        return { output.begin(), output.end() };
    }

    FVCORE_API std::u8string u8string(const std::u32string& input, bool strict) {
        auto length = input.size();
        std::vector<char8_t> output;
        output.reserve(length * 4);

        if (convertUTF32toUTF8(input.data(),
                               input.data() + length,
                               strict,
                               [&](char8_t ch) {
                                   output.push_back(ch);
                               }) == false) {
            return {};
        }
        return { output.begin(), output.end() };
    }

    FVCORE_API std::u16string u16string(const std::string& input, bool strict) {
        return u16string(std::u8string(input.begin(), input.end()), strict);
    }

    FVCORE_API std::u16string u16string(const std::wstring& input, bool strict) {
        return u16string(WNativeString::get(input), strict);
    }

    FVCORE_API std::u16string u16string(const std::u8string& input, bool strict) {
        auto length = input.size();
        std::vector<char16_t> output;
        output.reserve(length);

        if (convertUTF8toUTF16(input.data(),
                               input.data() + length,
                               strict,
                               [&](char16_t ch) {
                                   output.push_back(ch);
                               }) == false) {
            return {};
        }
        return { output.begin(), output.end() };
    }

    FVCORE_API std::u16string u16string(const std::u16string& input, bool strict) {
        return { input.c_str() };
    }

    FVCORE_API std::u16string u16string(const std::u32string& input, bool strict) {
        auto length = input.size();
        std::vector<char16_t> output;
        output.reserve(length);

        if (convertUTF32toUTF16(input.data(),
                                input.data() + length,
                                strict,
                                [&](char16_t ch) {
                                    output.push_back(ch);
                                }) == false) {
            return {};
        }
        return { output.begin(), output.end() };
    }

    FVCORE_API std::u32string u32string(const std::string& input, bool strict) {
        return u32string(std::u8string(input.begin(), input.end()), strict);
    }

    FVCORE_API std::u32string u32string(const std::wstring& input, bool strict) {
        return u32string(WNativeString::get(input), strict);
    }

    FVCORE_API std::u32string u32string(const std::u8string& input, bool strict) {
        auto length = input.size();
        std::vector<char32_t> output;
        output.reserve(length);

        if (convertUTF8toUTF32(input.data(),
                               input.data() + length,
                               strict,
                               [&](char32_t ch) {
                                   output.push_back(ch);
                               }) == false) {
            return {};
        }
        return { output.begin(), output.end() };
    }

    FVCORE_API std::u32string u32string(const std::u16string& input, bool strict) {
        auto length = input.size();
        std::vector<char32_t> output;
        output.reserve(length);

        if (convertUTF16toUTF32(input.data(),
                                input.data() + length,
                                strict,
                                [&](char32_t ch) {
                                    output.push_back(ch);
                                }) == false) {
            return {};
        }
        return { output.begin(), output.end() };
    }

    FVCORE_API std::u32string u32string(const std::u32string& input, bool strict) {
        return { input.c_str() };
    }
}
