#ifndef LOG_H
#define LOG_H

#include "lex.h"
#include <stdio.h>
#define LOG_DEBUG(fmt, ...) { fprintf(stdout, "[DEBUG] %s:%i:%s: ", __FILE__, __LINE__, __func__); fprintf(stdout, fmt, ##__VA_ARGS__); fprintf(stdout, "\n"); }
#define LOG_ERROR(fmt, ...) { fprintf(stderr, "[ERROR] %s:%i:%s: ", __FILE__, __LINE__, __func__); fprintf(stderr, fmt, ##__VA_ARGS__); fprintf(stderr, "\n"); }

void c_error(const lexeme_t *lexeme, const char *fmt, ...);
void c_debug(const char *fmt, ...);

#endif
