#include "Application.h"

#ifdef _WIN32
#include "Private/Win32/Application.h"
#endif

using namespace FV;

void Application::terminate(int exitCode)
{
#ifdef _WIN32
    Win32::terminateApplication(exitCode);
#endif
}

int Application::run()
{
#ifdef _WIN32
    return Win32::runApplication(this);
#endif
    return 0;
}
