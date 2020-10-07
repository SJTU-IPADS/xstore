#include "logging.hpp"

#include <iostream>

using namespace std;

namespace r2 {

// control flags for color
enum
{
  R_BLACK = 39,
  R_RED = 31,
  R_GREEN = 32,
  R_YELLOW = 33,
  R_BLUE = 34,
  R_MAGENTA = 35,
  R_CYAN = 36,
  R_WHITE = 37
};

std::string
StripBasename(const std::string& full_path)
{
  const char kSeparator = '/';
  size_t pos = full_path.rfind(kSeparator);
  if (pos != std::string::npos) {
    return full_path.substr(pos + 1, std::string::npos);
  } else {
    return full_path;
  }
}

inline string
EndcolorFlag()
{
  char flag[7];
  snprintf(flag, 7, "%c[0m", 0x1B);
  return string(flag);
}

const int RTX_DEBUG_LEVEL_COLOR[] = { R_BLACK,   R_YELLOW, R_BLACK, R_GREEN,
                                      R_MAGENTA, R_RED,    R_RED };

DisplayLogger::DisplayLogger(const char* file, int line, int level)
  : level_(level)
{
  if (level_ < ROCC_LOG_LEVEL)
    return;
}

MessageLogger::MessageLogger(const char* file, int line, int level)
  : DisplayLogger(file, line, level)
{
  stream_ << "[" << StripBasename(std::string(file)) << ":" << line << "] ";
}

DisplayLogger::~DisplayLogger()
{
  if (level_ >= ROCC_LOG_LEVEL) {
    stream_ << "\n";
    std::cout << "\033[" << RTX_DEBUG_LEVEL_COLOR[std::min(level_, 6)] << "m"
              << stream_.str() << EndcolorFlag() << std::flush;
    if (level_ >= LOG_FATAL)
      abort();
  }
}

} // namespace r2
