#include <algorithm>
#include "Logger.h"

using namespace FV;

std::map<Logger*, std::weak_ptr<Logger>> Logger::boundLoggers = {};
std::map<std::string, std::vector<std::weak_ptr<Logger>>> Logger::categorizedLoggers = {};
SpinLock Logger::lock = {};

Logger* Logger::defaultLogger()
{
    static std::shared_ptr<Logger> logger = std::make_shared<Logger>("FVCore", true);
    return logger.get();
}

Logger::Logger(const std::string& cat, bool bind)
    : category(cat), isBound(bind)
{
    auto key = this;
    std::scoped_lock guard(lock);
    if (bind)
    {
        boundLoggers[key] = weak_from_this();
    }
    if (categorizedLoggers.find(category) == categorizedLoggers.end())
        categorizedLoggers[category] = {};
    categorizedLoggers[category].push_back(weak_from_this());
}

Logger::~Logger()
{
    auto key = this;
    std::scoped_lock guard(lock);
    std::vector<std::weak_ptr<Logger>>& loggers = categorizedLoggers[category];
    std::vector<std::shared_ptr<Logger>> activeLoggers;
    activeLoggers.reserve(loggers.size());
    for (auto logger : loggers)
    {
        if (std::shared_ptr<Logger> al = logger.lock())
            activeLoggers.push_back(al);
    }
    loggers.clear();
    for (auto logger : activeLoggers)
    {
        loggers.push_back(logger);
    }
    if (loggers.empty())
    {
        categorizedLoggers.erase(category);
    }
    boundLoggers.erase(key);
}

std::vector<std::shared_ptr<Logger>> Logger::categorized(const std::string& category)
{
    std::scoped_lock guard(lock);
    std::vector<std::weak_ptr<Logger>>& loggers = categorizedLoggers[category];
    std::vector<std::shared_ptr<Logger>> activeLoggers;
    activeLoggers.reserve(loggers.size());
    for (auto logger : loggers)
    {
        if (std::shared_ptr<Logger> al = logger.lock())
            activeLoggers.push_back(al);
    }
    return activeLoggers;
}

void Logger::log(Level level, const std::string& mesg) const
{
    printf("[%s] %s\n", category.c_str(), mesg.c_str());
}

void Logger::broadcast(Level level, const std::string& mesg)
{
    std::vector<std::shared_ptr<Logger>> activeLoggers;
    do {
        std::scoped_lock guard(lock);
        for (auto it = boundLoggers.begin(); it != boundLoggers.end(); ++it)
        {
            if (std::shared_ptr<Logger> logger = it->second.lock())
                activeLoggers.push_back(logger);
        }
    } while (false);
    std::for_each(activeLoggers.begin(), activeLoggers.end(),
                  [&](std::shared_ptr<Logger>& logger)
                  {
                      logger->log(level, mesg);
                  });
}
