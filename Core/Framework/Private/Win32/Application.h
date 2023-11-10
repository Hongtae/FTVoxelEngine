#pragma once
#include <functional>
#include <vector>
#include <string>
#include "../../Application.h"

#ifdef _WIN32
namespace FV::Win32 {
    int runApplication(Application*);
    void terminateApplication(int exitCode);

    std::vector<std::u8string> commandLineArguments();
    std::u8string environmentPath(Application::EnvironmentPath);
}
#endif //#ifdef _WIN32
