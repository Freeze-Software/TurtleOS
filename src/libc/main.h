#include <stdint.h>
#include <stdarg.h>

int vsprintf(char *out, const char *fmt, va_list args);
int sprintf(char *out, const char *fmt, ...);
char *itoa(int value, char *str, int base);
uint32_t string_to_hex(const char *str);
int memcmp(const void *a, const void *b, uint32_t n);
char *strchr(const char *str, int c);
char *strncpy(char *dst, const char *src, uint32_t n);
void *memset(void *dst, int val, uint32_t n);
int strncmp(const char *a, const char *b, uint32_t n);
int strlen(char* str);
int strnlen(char* str, int len);
void* memcpy(void* dest, const void* src, unsigned int n);
