#pragma once
#include "../include.h"
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <format>
#include "SpinLock.h"

namespace FV
{
    class Logger : public std::enable_shared_from_this<Logger>
    {
    public:
        enum class Level
        {
            debug, verbose, info, warning, error
        };

        Logger(const std::string& category, bool bind);
        virtual ~Logger();
        
        static std::vector<std::shared_ptr<Logger>> categorized(const std::string&);
        virtual void log(Level level, const std::string& mesg) const;
        static void broadcast(Level level, const std::string& mesg);

        static Logger* defaultLogger();

    private:
        static std::map<Logger*, std::weak_ptr<Logger>> boundLoggers;
        static std::map<std::string, std::vector<std::weak_ptr<Logger>>> categorizedLoggers;
        static SpinLock lock;

        std::string category;
        bool isBound;
    };

    struct Log
    {
        using Level = Logger::Level;

        static void log(const std::string& category, Level level, const std::string& mesg)
        {
            for (auto logger : Logger::categorized(category))
            {
                logger->log(level, mesg);
            }
        }

        static void log(Level level, const std::string& mesg)
        {
            Logger* d = Logger::defaultLogger();
            Logger::broadcast(level, mesg);
        }

        static void debug(const std::string& mesg)     { log(Level::debug, mesg); }
        static void verbose(const std::string& mesg)   { log(Level::verbose, mesg); }
        static void info(const std::string& mesg)      { log(Level::info, mesg); }
        static void warning(const std::string& mesg)   { log(Level::warning, mesg); }
        static void error(const std::string& mesg)     { log(Level::error, mesg); }
    };
}
