#include "Application.h"
#include "Unicode.h"

#ifdef _WIN32
#include "Private/Win32/Application.h"
#endif

namespace {
    FV::Application* shared;
    std::vector<std::u8string> commandLineArgs;
}

using namespace FV;

void Application::terminate(int exitCode) {
#ifdef _WIN32
    Win32::terminateApplication(exitCode);
#endif
}

int Application::run() {
    std::vector<std::u8string> args;
#ifdef _WIN32
    args = Win32::commandLineArguments();
#endif
    return run(args);
}

int Application::run(int argc, char8_t** argv) {
    std::vector<std::u8string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i)
        args.push_back(argv[i]);
    return run(args);
}

int Application::run(int argc, wchar_t** argv) {
    std::vector<std::u8string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i)
        args.push_back(u8string(std::wstring(argv[i])));
    return run(args);
}

int Application::run(std::vector<std::u8string> args) {
    commandLineArgs = std::move(args);
    shared = this;

    int result;
#ifdef _WIN32
    result = Win32::runApplication(this);
#endif
    shared = nullptr;
    return result;
}

std::vector<std::u8string> Application::commandLineArguments() {
    return commandLineArgs;
}

Application* Application::sharedInstance() {
    return shared;
}

std::filesystem::path Application::environmentPath(EnvironmentPath path) {
#ifdef _WIN32
    return Win32::environmentPath(path);
#endif
    return {};
}
