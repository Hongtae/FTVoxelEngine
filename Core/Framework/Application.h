#pragma once
#include "../include.h"
#include <vector>
#include <string>

namespace FV
{
    class FVCORE_API Application
    {
    public:
        enum class EnvironmentPath
        {
            SystemRoot,			// system root. (boot volume on Windows)
            AppRoot,			// root directory of executable.
            AppResource,		// application resource directory.
            AppExecutable,		// directory path where executable is.
            AppData,			// application's data directory.
            UserHome,			// home directory path for current user.
            UserDocuments,		// user's document directory.
            UserPreferences,	// user's preferences(config) directory.
            UserCache,			// user's cache directory.
            UserTemp,			// temporary directory for current user.
        };

        virtual ~Application() {}

        virtual void initialize() {}
        virtual void finalize() {}

        void terminate(int exitCode);
        int run();
        int run(int argc, char** argv);
        int run(std::vector<std::string>);

        static Application* sharedInstance();
        static std::vector<std::string> commandLineArguments();

        static std::string environmentPath(EnvironmentPath);
    };
}
