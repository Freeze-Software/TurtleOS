#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

char *itoa(int value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp;
    int negative = 0;

    if (base < 2 || base > 16) {
        *str = '\0';
        return str;
    }

    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }

    if (value < 0 && base == 10) {
        negative = 1;
        value = -value;
    }

    while (value) {
        int digit = value % base;
        *ptr++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    }

    if (negative)
        *ptr++ = '-';

    *ptr-- = '\0';

    while (ptr1 < ptr) {
        tmp = *ptr;
        *ptr = *ptr1;
        *ptr1 = tmp;
        ptr--;
        ptr1++;
    }

    return str;
}

void* memcpy(void* dest, const void* src, unsigned int n) {
    uint32_t* d32 = (uint32_t*)dest;
    const uint32_t* s32 = (const uint32_t*)src;

    while (n >= 4) {
        *d32++ = *s32++;
        n -= 4;
    }

    uint8_t* d8 = (uint8_t*)d32;
    const uint8_t* s8 = (const uint8_t*)s32;

    while (n--) {
        *d8++ = *s8++;
    }

    return dest;
}

char *strchr(const char *str, int c) {
    while (*str)
    {
        if (*str == (char)c)
            return (char *)str;

        str++;
    }

    if (c == '\0')
        return (char *)str;

    return NULL;
}

int strncmp(const char *a, const char *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == 0)
            return 0;
    }
    return 0;
}

int memcmp(const void *a, const void *b, uint32_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (uint32_t i = 0; i < n; i++) {
        if (pa[i] < pb[i]) return -1;
        if (pa[i] > pb[i]) return  1;
    }
    return 0;
}

void *memset(void *dst, int val, uint32_t n) {
    uint8_t *p = (uint8_t *)dst;
    for (uint32_t i = 0; i < n; i++)
        p[i] = (uint8_t)val;
    return dst;
}

char *strncpy(char *dst, const char *src, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = 0;
    return dst;
}

int strlen(char* str) {
	int len = 0;
	for (int i = 0; str[i] != '\0'; i++) {
		len++;
	}
	return len;
}

int strnlen(char* str, int maxlen) {
	int len = 0;
	for (int i = 0; str[i] != '\0' && len < maxlen; i++) {
		len++;
	}
	return len;
}

uint32_t string_to_hex(const char *str) {
    uint32_t result = 0;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }

    while (*str) {
        result <<= 4;

        if (*str >= '0' && *str <= '9') {
            result |= (*str - '0');
        }
        else if (*str >= 'A' && *str <= 'F') {
            result |= (*str - 'A' + 10);
        }
        else if (*str >= 'a' && *str <= 'f') {
            result |= (*str - 'a' + 10);
        }
        else {
            break;
        }

        str++;
    }

    return result;
}

int vsprintf(char *out, const char *fmt, va_list args)
{
    char buffer[32];
    int pos = 0;

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            i++;

            switch (fmt[i]) {
            case 's': {
                char *s = va_arg(args, char *);
                while (*s)
                    out[pos++] = *s++;
                break;
            }

            case 'd':
                itoa(va_arg(args, int), buffer, 10);
                for (int j = 0; buffer[j]; j++)
                    out[pos++] = buffer[j];
                break;

            case 'x':
                itoa(va_arg(args, int), buffer, 16);
                for (int j = 0; buffer[j]; j++)
                    out[pos++] = buffer[j];
                break;

            case 'c':
                out[pos++] = (char)va_arg(args, int);
                break;

            case '%':
                out[pos++] = '%';
                break;
            }
        } else {
            out[pos++] = fmt[i];
        }
    }

    out[pos] = '\0';
    return pos;
}

int sprintf(char *out, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int len = vsprintf(out, fmt, args);

    va_end(args);
    return len;
}
