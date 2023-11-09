#pragma once
#include "../include.h"
#include <map>
#include <vector>

namespace FV {
    class Logger : public std::enable_shared_from_this<Logger> {
    public:
        enum class Level {
            Debug, Verbose, Info, Warning, Error
        };

        Logger(const std::string& category);
        virtual ~Logger();

        void bind(bool retain);
        void unbind();
        bool isBound() const;

        static std::vector<std::shared_ptr<Logger>> categorized(const std::string&);
        virtual void log(Level level, const std::string& mesg) const;
        static void broadcast(Level level, const std::string& mesg);

        static Logger* defaultLogger();

    private:
        const std::string category;
    };

    struct Log {
        using Level = Logger::Level;

        static void log(const std::string& category, Level level, const std::string& mesg) {
            for (auto logger : Logger::categorized(category)) {
                logger->log(level, mesg);
            }
        }

        static void log(Level level, const std::string& mesg) {
            Logger* d = Logger::defaultLogger();
            Logger::broadcast(level, mesg);
        }

        static void debug(const std::string& mesg) { log(Level::Debug, mesg); }
        static void verbose(const std::string& mesg) { log(Level::Verbose, mesg); }
        static void info(const std::string& mesg) { log(Level::Info, mesg); }
        static void warning(const std::string& mesg) { log(Level::Warning, mesg); }
        static void error(const std::string& mesg) { log(Level::Error, mesg); }
    };
}
