#include "Logger.h"
#include "VirtualKey.h"

#ifdef _WIN32
#include <windows.h>
#include <wincon.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>

using namespace FV::Win32;

Logger::Logger()
    : console(nullptr)
    , allocatedConsole(false)
    , initTextAttrs(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED)
    , FV::Logger("Win32")
{
    allocatedConsole = ::AllocConsole();

    if (allocatedConsole)
    {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);

        // stdio redirect
        int fin = _open_osfhandle((intptr_t)hIn, _O_TEXT);
        int fout = _open_osfhandle((intptr_t)hOut, _O_TEXT);
        int ferr = _open_osfhandle((intptr_t)hErr, _O_TEXT);

        *stdin = *_fdopen(fin, "r");
        *stdout = *_fdopen(fout, "w");
        *stderr = *_fdopen(ferr, "w");
        setvbuf(stderr, NULL, _IONBF, 0);

        std::ios::sync_with_stdio();

        console = hOut;
    }
    else
    {
        console = ::GetStdHandle(STD_OUTPUT_HANDLE);
    }
    if (console)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(console, &info))
            initTextAttrs = info.wAttributes;
    }
}

Logger::~Logger()
{
    if (console)
    {
        ::SetConsoleTextAttribute(console, initTextAttrs);

        if (allocatedConsole)
        {
            if (::IsDebuggerPresent() == false)
                system("pause");
            //::FreeConsole();
        }
    }
}

void Logger::log(Level level, const std::string& mesg) const
{
    WORD attr;
    const char* header = "";
    switch (level)
    {
    case Level::Debug:
        attr = FOREGROUND_GREEN;
        header = "D";
        break;
    case Level::Verbose:
        attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        header = "V";
        break;
    case Level::Info:
        attr = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        header = "I";
        break;
    case Level::Warning:
        attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        header = "W";
        break;
    case Level::Error:
        attr = FOREGROUND_RED | FOREGROUND_INTENSITY;
        header = "E";
        break;
    default:
        attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    }

    std::string mesg2 = std::format("[{:d}:{:d}/{}] {}",
                                   (uint32_t)GetCurrentProcessId(),
                                   (uint32_t)GetCurrentThreadId(),
                                   header,
                                   mesg);

    if (mesg2.ends_with("\n") == false)
        mesg2 += "\n";

    auto ws = toUTF16(mesg2);
    ::OutputDebugStringW(ws.c_str());
    writeLog(attr, ws.c_str());
}

void Logger::writeLog(WORD attr, const char* str) const
{
    if (console)
    {
        SetConsoleTextAttribute(console, attr);
        DWORD dwWritten = 0;
        WriteConsoleA(console, str, (DWORD)strlen(str), &dwWritten, 0);
        return;
    }
    else
        printf(str);
}

void Logger::writeLog(WORD attr, const wchar_t* str) const
{
    if (console)
    {
        SetConsoleTextAttribute(console, attr);
        DWORD dwWritten = 0;
        WriteConsoleW(console, str, (DWORD)wcslen(str), &dwWritten, 0);
        return;
    }
    else
        printf("%ls", str);
}

#endif //#ifdef _WIN32
