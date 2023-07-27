#include "Application.h"

#ifdef _WIN32
#include "Private/Win32/Application.h"
#endif

using namespace FV;

void Application::terminate(int exitCode)
{
}

int Application::run()
{
#ifdef _WIN32
    return Win32::runApplication(this);
#endif
    return 0;
}
