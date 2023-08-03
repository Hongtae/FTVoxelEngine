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

    }

    void finalize() override
    {

    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    return RenderTestApp().run();
}

