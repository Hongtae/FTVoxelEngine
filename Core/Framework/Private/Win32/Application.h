#pragma once
#include <functional>
#include <vector>
#include <string>
#include "../../Application.h"

#ifdef _WIN32
namespace FV::Win32
{
    int runApplication(Application*);
    void terminateApplication(int exitCode);

    void postOperation(std::function<void()>);

    std::vector<std::string> commandLineArguments();
}
#endif //#ifdef _WIN32
