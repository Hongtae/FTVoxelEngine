#include <algorithm>
#include <mutex>
#include <map>
#include <set>
#include <vector>
#include "Logger.h"

using namespace FV;

namespace {
    std::map<Logger*, std::weak_ptr<Logger>> boundLoggers;
    std::map<Logger*, std::shared_ptr<Logger>> retainedLoggers;
    std::map<std::string, std::vector<Logger*>> categorizedLoggers;
    std::mutex lock;
}

Logger* Logger::defaultLogger() {
    static std::shared_ptr<Logger> logger = std::make_shared<Logger>("Core");
    return logger.get();
}

Logger::Logger(const std::string& cat)
    : category(cat) {
    std::scoped_lock guard(lock);
    categorizedLoggers.try_emplace(category);
    categorizedLoggers[category].push_back(this);
}

Logger::~Logger() {
    std::scoped_lock guard(lock);
    std::vector<Logger*>& loggers = categorizedLoggers[category];
    auto it = std::find_if(loggers.begin(), loggers.end(), [this](auto p) {
        return this == p;
                           });
    FVASSERT_DEBUG(it != loggers.end());
    loggers.erase(it);
    if (loggers.empty()) {
        categorizedLoggers.erase(category);
    }
    boundLoggers.erase(this);
}

void Logger::bind(bool retain) {
    std::scoped_lock guard(lock);
    boundLoggers[this] = weak_from_this();
    if (retain)
        retainedLoggers[this] = shared_from_this();
}

void Logger::unbind() {
    // Prevent calling a destructor while it still owns the lock
    auto guard = shared_from_this();
    do {
        std::scoped_lock guard(lock);
        boundLoggers.erase(this);
        retainedLoggers.erase(this);
    } while (false);
}

bool Logger::isBound() const {
    std::scoped_lock guard(lock);
    return boundLoggers.contains(const_cast<Logger*>(this));
}

std::vector<std::shared_ptr<Logger>> Logger::categorized(const std::string& category) {
    std::scoped_lock guard(lock);
    std::vector<Logger*>& loggers = categorizedLoggers[category];
    std::vector<std::shared_ptr<Logger>> activeLoggers;
    activeLoggers.reserve(loggers.size());
    for (auto logger : loggers) {
        if (std::shared_ptr<Logger> al = logger->shared_from_this())
            activeLoggers.push_back(al);
    }
    return activeLoggers;
}

void Logger::log(Level level, const std::string& mesg) const {
    printf("[%s] %s\n", category.c_str(), mesg.c_str());
}

void Logger::broadcast(Level level, const std::string& mesg) {
    std::vector<std::shared_ptr<Logger>> activeLoggers;
    do {
        std::scoped_lock guard(lock);
        for (auto it = boundLoggers.begin(); it != boundLoggers.end(); ++it) {
            if (std::shared_ptr<Logger> logger = it->second.lock())
                activeLoggers.push_back(logger);
        }
    } while (false);
    std::for_each(activeLoggers.begin(), activeLoggers.end(),
                  [&](std::shared_ptr<Logger>& logger) {
                      logger->log(level, mesg);
                  });
}
