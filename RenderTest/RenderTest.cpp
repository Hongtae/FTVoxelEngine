// RenderTest.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "RenderTest.h"

using namespace FV;

class RenderTestApp : public Application
{
public:
    void initialize() override
    {
        Log::debug("OnInitialize");

        this->terminate(1234);
    }

    void finalize() override
    {
        Log::debug("OnFinalize");
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    return RenderTestApp().run();
}
