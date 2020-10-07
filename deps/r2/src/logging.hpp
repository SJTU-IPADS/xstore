#pragma once

#include "common.hpp"

#include <cstring>
#include <sstream>
#include <string>
#include <cstdarg>
#include <fstream>

namespace r2 {

/**
 * \def LOG_FATAL
 *   Used for fatal and probably irrecoverable conditions
 * \def LOG_ERROR
 *   Used for errors which are recoverable within the scope of the function
 * \def LOG_WARNING
 *   Logs interesting conditions which are probably not fatal
 * \def LOG_EMPH
 *   Outputs as LOG_INFO, but in LOG_WARNING colors. Useful for
 *   outputting information you want to emphasize.
 * \def LOG_INFO
 *   Used for providing general useful information
 * \def LOG_DEBUG
 *   Debugging purposes only
 * \def LOG_EVERYTHING
 *   Log everything
 */

enum loglevel {
  LOG_NONE       = 7,
  LOG_FATAL      = 6,
  LOG_ERROR      = 5,
  LOG_WARNING    = 4,
  LOG_EMPH       = 3,
  LOG_INFO       = 2,
  LOG_DEBUG      = 1,
  LOG_EVERYTHING = 0
};

#ifndef ROCC_LOG_LEVEL
#define ROCC_LOG_LEVEL ::r2::LOG_INFO
#endif

// logging macro definiations
// default log

#define DISPLAY(n)                                              \
  if (n >= ROCC_LOG_LEVEL)                                      \
    ::r2::DisplayLogger((char*)__FILE__, __LINE__, n).stream()

#define LOG(n)                                                  \
  if (n >= ROCC_LOG_LEVEL)                                      \
    ::r2::MessageLogger((char*)__FILE__, __LINE__, n).stream()

// log with tag
#define TLOG(n,t)                                               \
  if(n >= ROCC_LOG_LEVEL)                                       \
    ::r2::MessageLogger((char*)__FILE__, __LINE__, n).stream()  \
          << "[" << (t) << "]"

#define LOG_IF(n,condition)                                     \
  if(n >= ROCC_LOG_LEVEL && (condition))                        \
    ::r2::MessageLogger((char*)__FILE__, __LINE__, n).stream()

#define ASSERT(condition)                                               \
  if(unlikely(!(condition)))                                            \
    ::r2::MessageLogger((char*)__FILE__, __LINE__, ::r2::LOG_FATAL + 1).stream() << "Assertion! "

#define VERIFY(n,condition) LOG_IF(n,(!(condition)))

#define FILE_LOG(f)                             \
  ::r2::FileLogger(f).stream()

#define FILE_WRITE(f,m)                            \
  ::r2::FileLogger(f,m).stream()

// a nice progrss printer
// credits: razzak@stackoverflow
inline ALWAYS_INLINE
void PrintProgress(double percentage, const char *header = NULL,FILE *out = stdout) {

#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 60
  int val = (int) (percentage * 100);
  int lpad = (int) (percentage * PBWIDTH);
  int rpad = PBWIDTH - lpad;

  const char *head_padding = header != NULL? header:"";

  fprintf(out,"\r%s %3d%% [%.*s%*s]", head_padding,val, lpad, PBSTR, rpad, "");
  if(percentage == 100)
    printf("\n");
}

class DisplayLogger {
 public:
  DisplayLogger(const char *file, int line, int level);
  ~DisplayLogger();

  // Return the stream associated with the logger object.
  std::stringstream &stream() { return stream_; }
 protected:
  std::stringstream stream_;
  int level_;
};

class FileLogger {
 public:
  FileLogger(const char *filename, std::ios_base::openmode mode = std::ofstream::app) :
      stream_(filename,mode) {
  }

  FileLogger(const std::string &str,std::ios_base::openmode mode = std::ofstream::app)
      : FileLogger(str.c_str(),mode) {

  }

  ~FileLogger() {
    stream_ << "\n";
    stream_.close();
  }
  std::ofstream &stream() { return stream_; }
 private:
  std::ofstream stream_;
};

class MessageLogger : public DisplayLogger {
 public:
  MessageLogger(const char *file, int line, int level);
};

inline void MakeStringInternal(std::stringstream& /*ss*/) {}

template <typename T>
inline void MakeStringInternal(std::stringstream& ss, const T& t) {
  ss << t;
}

template <typename T, typename... Args>
inline void
MakeStringInternal(std::stringstream& ss, const T& t, const Args&... args) {
  MakeStringInternal(ss, t);
  MakeStringInternal(ss, args...);
}

template <typename... Args>
inline std::string MakeString(const Args&... args) {
  std::stringstream ss;
  MakeStringInternal(ss, args...);
  return std::string(ss.str());
}

// Specializations for already-a-string types.
template <>
inline std::string MakeString(const std::string& str) {
  return str;
}
inline std::string MakeString(const char* c_str) {
  return std::string(c_str);
}

} // namespace r2
