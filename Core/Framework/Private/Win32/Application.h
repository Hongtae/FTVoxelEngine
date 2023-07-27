#pragma once
#include <functional>
#include "../../Application.h"

#ifdef _WIN32
namespace FV::Win32
{
    int runApplication(Application*);
    void terminateApplication(int exitCode);

    void postOperation(std::function<void()>);
}
#endif //#ifdef _WIN32
