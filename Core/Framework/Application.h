#pragma once
#include "../include.h"
#include <vector>
#include <string>

namespace FV
{
    class FVCORE_API Application
    {
    public:
        virtual ~Application() {}

        virtual void initialize() {}
        virtual void finalize() {}

        void terminate(int exitCode);
        int run();
        int run(int argc, char** argv);
        int run(std::vector<std::string>);

        static Application* sharedInstance();
        static std::vector<std::string> commandLineArguments();
    };
}
