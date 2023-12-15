#pragma once
#include <functional>
#include <vector>
#include <string>
#include "../../Application.h"

#ifdef _WIN32
namespace FV {
    struct Win32App {
        static int runApplication(Application*);
        static void terminateApplication(int exitCode);
        static std::vector<std::u8string> commandLineArguments();
        static std::u8string environmentPath(Application::EnvironmentPath);
    };
}
#endif //#ifdef _WIN32
