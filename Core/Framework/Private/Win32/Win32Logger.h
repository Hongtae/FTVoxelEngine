#pragma once
#include "../../Logger.h"

#ifdef _WIN32
#include <Windows.h>

namespace FV {
    class Win32Logger : public Logger {
    public:
        Win32Logger();
        ~Win32Logger();

        void log(Level level, const std::string& mesg) const override;

        void writeLog(WORD attr, const wchar_t* str) const;

        HANDLE console;
        WORD initTextAttrs;
        bool allocatedConsole;
    };
}

#endif //#ifdef _WIN32
