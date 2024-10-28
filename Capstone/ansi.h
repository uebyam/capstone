#ifndef ANSI_H
#define ANSI_H

#define LOG_CLEARFMT() printf("\033[m")
#define LOG_DEBUG_NOFMT(...) printf(__VA_ARGS__)
#define LOG_INFO_NOFMT(...) printf(__VA_ARGS__)
#define LOG_WARN_NOFMT(...) printf(__VA_ARGS__)
#define LOG_ERR_NOFMT(...) printf(__VA_ARGS__)
#define LOG_FATAL_NOFMT(...) printf(__VA_ARGS__)
#define LOG_DEBUG(...) printf("\033[0;1;90m[DEBUG %s]\033[0;90m ", __func__); printf(__VA_ARGS__)
#define LOG_INFO(...) printf("\033[0;1;39m[INFO  %s]\033[0;39m ", __func__); printf(__VA_ARGS__)
#define LOG_WARN(...) printf("\033[0;1;33m[WARN  %s]\033[0;33m ", __func__); printf(__VA_ARGS__)
#define LOG_ERR(...) printf("\033[0;1;91m[ERROR %s]\033[0;91m ", __func__); printf(__VA_ARGS__)
#define LOG_FATAL(...) printf("\033[0;1;38;101m[FATAL %s]\033[0;91m ", __func__); printf(__VA_ARGS__)

/*
#define LOG_CLEARFMT()
#define LOG_DEBUG_NOFMT(...)
#define LOG_INFO_NOFMT(...)
#define LOG_WARN_NOFMT(...)
#define LOG_ERR_NOFMT(...)
#define LOG_FATAL_NOFMT(...)
#define LOG_DEBUG(...)
#define LOG_INFO(...)
#define LOG_WARN(...)
#define LOG_ERR(...)
#define LOG_FATAL(...)
*/

#endif
