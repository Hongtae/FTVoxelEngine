#pragma once
#include "../../Logger.h"

#ifdef _WIN32
#include <Windows.h>

namespace FV::Win32
{
    class Logger : public FV::Logger
    {
    public:
        Logger();
        ~Logger();

        void log(Level level, const std::string& mesg) const override;

        void writeLog(WORD attr, const char* str) const;
        void writeLog(WORD attr, const wchar_t* str) const;

        HANDLE console;
        WORD initTextAttrs;
        bool allocatedConsole;
    };
}

#endif //#ifdef _WIN32
