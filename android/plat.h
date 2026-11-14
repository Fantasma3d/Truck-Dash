/* Declaracoes minimas de libc: no Android compilamos sem NDK (so clang + stubs) */
#ifndef PLAT_H
#define PLAT_H
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#ifdef __ANDROID__
int snprintf(char *, size_t, const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);
size_t strlen(const char *);
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
char *strncpy(char *, const char *, size_t);
void *malloc(size_t);
void *calloc(size_t, size_t);
void free(void *);
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#endif
#endif
