#ifndef _MEMRAY_LOGGING_H
#define _MEMRAY_LOGGING_H

#include <sstream>
#include <string>

namespace memray {

#define __MY_DEBUG
#ifdef __MY_DEBUG
#define MY_DEBUG(format, args...) printf("FILE: " __FILE__ ", LINE: %d: " format "\n", __LINE__, ##args)  // __VA_ARGS__ -> args
#else 
#define MY_DEBUG(format, ...) printf("FILE: " __FILE__ ", LINE: %d: " format "\n", __LINE__)
#endif

enum logLevel {
    NOTSET = 0,
    DEBUG = 10,
    INFO = 20,
    WARNING = 30,
    ERROR = 40,
    CRITICAL = 50,
};

void
logToStderr(const std::string& message, int level);

void
setLogThreshold(int threshold);

logLevel
getLogThreshold();

class LOG
{
  public:
    // Constructors
    LOG()
    : msgLevel(INFO){};

    explicit LOG(logLevel type)
    {
        msgLevel = type;
    };

    // Destructors
    ~LOG()
    {
        logToStderr(buffer.str(), msgLevel);
    };

    // Operators
    template<typename T>
    LOG& operator<<(const T& msg)
    {
        if (msgLevel < getLogThreshold()) {
            return *this;
        }
        buffer << msg;
        return *this;
    };

  private:
    // Data members
    std::ostringstream buffer;
    logLevel msgLevel = DEBUG;
};

}  // namespace memray

#endif  //_MEMRAY_LOGGING_H
