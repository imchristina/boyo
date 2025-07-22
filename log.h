#ifndef LOG_H
#define LOG_H

#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_BLUE "\x1b[34m"
#define ANSI_BLUE_BOLD "\x1b[1;34m"
#define ANSI_CLEAR "\x1b[0m"

#define DEBUG_PRINTF(...) //printf(__VA_ARGS__)
#define DEBUG_PRINTF_CPU(...) //printf(__VA_ARGS__)
#define DEBUG_PRINTF_MEM(...) //printf(__VA_ARGS__)
#define DEBUG_PRINTF_PPU(...) //printf(__VA_ARGS__)

#endif
