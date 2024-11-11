#ifndef ANSI_H
#define ANSI_H

#include <stdio.h>

// LOG LEVELS
// Each log level implies the presence of messages from all log levels below it.
//
// 0: Absolutely no logs
// 1: Only fatal/showstopping errors
// 2: Any error, including those that can be recovered from
// 3: Warnings and errors
// 4: Info about what the code is doing
// 5: Even more info about what the code is doing


#define LOGLEVEL 5

#if (LOGLEVEL > 0)
#define LOG_CLEARFMT() printf("\033[m")
#define LOG_FATAL_NOFMT(...) printf(__VA_ARGS__)
#define LOG_FATAL(...) printf("\033[0;1;38;101m[FATAL %s]\033[0;91m ", __func__); printf(__VA_ARGS__)
#else
#define LOG_CLEARFMT()
#define LOG_FATAL_NOFMT(...)
#define LOG_FATAL(...)
#endif

#if (LOGLEVEL > 1)
#define LOG_ERR_NOFMT(...) printf(__VA_ARGS__)
#define LOG_ERR(...) printf("\033[0;1;91m[ERROR %s]\033[0;91m ", __func__); printf(__VA_ARGS__)
#else
#define LOG_ERR_NOFMT(...)
#define LOG_ERR(...)
#endif

#if (LOGLEVEL > 2)
#define LOG_WARN_NOFMT(...) printf(__VA_ARGS__)
#define LOG_WARN(...) printf("\033[0;1;33m[WARN  %s]\033[0;33m ", __func__); printf(__VA_ARGS__)
#else
#define LOG_WARN_NOFMT(...)
#define LOG_WARN(...)
#endif

#if (LOGLEVEL > 3)
#define LOG_INFO_NOFMT(...) printf(__VA_ARGS__)
#define LOG_INFO(...) printf("\033[0;1m[INFO  %s]\033[m ", __func__); printf(__VA_ARGS__)
#else
#define LOG_INFO_NOFMT(...)
#define LOG_INFO(...)
#endif

#if (LOGLEVEL > 4)
#define LOG_DEBUG_NOFMT(...) printf(__VA_ARGS__)
#define LOG_DEBUG(...) printf("\033[0;1;90m[DEBUG %s]\033[0;90m ", __func__); printf(__VA_ARGS__)
#else
#define LOG_DEBUG_NOFMT(...)
#define LOG_DEBUG(...)
#endif

#endif
