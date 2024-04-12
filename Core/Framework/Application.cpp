#include <thread>
#include <chrono>
#include "Application.h"
#include "Unicode.h"

#ifdef _WIN32
#include "Private/Win32/Win32Application.h"
#endif

using namespace FV;

namespace {
    Application* shared;
    std::vector<std::u8string> commandLineArgs;
}

void Application::terminate(int exitCode) {
#ifdef _WIN32
    Win32App::terminateApplication(exitCode);
#endif
}

int Application::run() {
    std::vector<std::u8string> args;
#ifdef _WIN32
    args = Win32App::commandLineArguments();
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
    result = Win32App::runApplication(this);
#endif
    auto yield = [](std::chrono::milliseconds t) {
        auto start = std::chrono::high_resolution_clock::now();
        auto end = start + t;
        do {
            std::this_thread::yield();
        } while (std::chrono::high_resolution_clock::now() < end);
    };
    yield(std::chrono::milliseconds(1));
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
    return Win32App::environmentPath(path);
#endif
    return {};
}
