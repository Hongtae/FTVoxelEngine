#pragma once
#include "../include.h"

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
    };
}