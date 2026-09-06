/* SPDX-License-Identifier: BSD-2-Clause */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_atomic.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/led/f133.h>
#include <sbi_utils/clock/f133.h>
#include <sbi_utils/serial/f133_uart.h>
#include <sbi_utils/display/f133_slcd.h>

#pragma GCC optimize ("O0")

#define CONSOLE_TBUF_MAX    64
#define PAD_RIGHT           1
#define PAD_ZERO            2
#define PAD_ALTERNATE       4
#define PAD_SIGN            8
#define USE_TBUF            16
#define PRINT_BUF_LEN       64
#define va_start(v, l)      __builtin_va_start((v), l)
#define va_end              __builtin_va_end
#define va_arg              __builtin_va_arg

typedef __builtin_va_list va_list;

static u32 console_tbuf_len;
static char console_tbuf[CONSOLE_TBUF_MAX];

void clrbits(u32 addr, u32 bits)
{
    writel(readl(addr) & ~bits, addr);
}

void setbits(u32 addr, u32 bits)
{
    writel(readl(addr) | bits, addr);
}

void clrsetbits(u32 addr, u32 clr, u32 set)
{
    clrbits(addr, clr);
    setbits(addr, set);
}

static inline u64 get_arch_counter(void)
{
     unsigned long long cnt = 0;

     asm volatile("csrr %0, time\n"
        : "=r"(cnt)
        :
        : "memory"
        );

    return cnt;
}

u32 get_sys_ticks(void)
{
    return (u32)get_arch_counter() / 24000;
}

u32 timer_get_us(void)
{
    return (u32)get_arch_counter() / 24;
}

void udelay(unsigned long us)
{
    u64 t1, t2;

    t1 = get_arch_counter();
    t2 = t1 + us * 24;
    do {
        t1 = get_arch_counter();
    } while (t2 >= t1);
}

void mdelay(u32 ms)
{
    udelay(ms * 1000);
}

void __usdelay(u32 us)
{
    udelay(us);
}

void __msdelay(u32 ms)
{
    mdelay(ms);
}

static void nputs_all(const char *str, u32 len)
{
    u32 i = 0;

    for (i = 0; i < len; i++) {
        if (str[i] == '\n') {
            f133_uart_putc('\r');
        }
        f133_uart_putc(str[i]);
    }
}

static void printc(char **out, u32 *out_len, char ch, int flags)
{
    if (!out) {
        f133_uart_putc(ch);
    }
    else {
        if (!out_len || *out_len > 1) {
            *(*out)++ = ch;
            **out = '\0';
            if (out_len) {
                --(*out_len);
                if ((flags & USE_TBUF) && *out_len == 1) {
                    nputs_all(console_tbuf, CONSOLE_TBUF_MAX - *out_len);
                    *out = console_tbuf;
                    *out_len = CONSOLE_TBUF_MAX;
                }
            }
        }
    }
}

static int prints(char **out, u32 *out_len, const char *string, int width, int flags)
{
    int pc = 0;
    int len = 0;

    for (len=0; string[len]; len++) {
        width -= 1;
    }

    if (!(flags & PAD_RIGHT)) {
        for (; width > 0; --width) {
            printc(out, out_len, flags & PAD_ZERO ? '0' : ' ', flags);
            ++pc;
        }
    }
    for (; *string; ++string) {
        printc(out, out_len, *string, flags);
        ++pc;
    }
    for (; width > 0; --width) {
        printc(out, out_len, ' ', flags);
        ++pc;
    }

    return pc;
}

static int printi(char **out, u32 *out_len, long long i, int width, int flags, int type)
{
    int pc = 0;
    char *s, sign = 0, letbase, print_buf[PRINT_BUF_LEN];
    unsigned long long u, b, t;

    b = 10;
    letbase = 'a';
    if (type == 'o') {
        b = 8;
    }
    else if (type == 'x' || type == 'X' || type == 'p' || type == 'P') {
        b = 16;
        letbase &= ~0x20;
        letbase |= type & 0x20;
    }

    u = i;
    sign = 0;
    if (type == 'i' || type == 'd') {
        if ((flags & PAD_SIGN) && i > 0) {
            sign = '+';
        }
        if (i < 0) {
            sign = '-';
            u = -i;
        }
    }

    s  = print_buf + PRINT_BUF_LEN - 1;
    *s = '\0';

    if (!u) {
        *--s = '0';
    } 
    else {
        while (u) {
            t = u % b;
            u = u / b;
            if (t >= 10) {
                t += letbase - '0' - 10;
            }
            *--s = t + '0';
        }
    }

    if (flags & PAD_ZERO) {
        if (sign) {
            printc(out, out_len, sign, flags);
            ++pc;
            --width;
        }
        if (i && (flags & PAD_ALTERNATE)) {
            if (b == 16 || b == 8) {
                printc(out, out_len, '0', flags);
                ++pc;
                --width;
            }
            if (b == 16) {
                printc(out, out_len, 'x' - 'a' + letbase, flags);
                ++pc;
                --width;
            }
        }
    }
    else {
        if (i && (flags & PAD_ALTERNATE)) {
            if (b == 16) {
                *--s = 'x' - 'a' + letbase;
            }
            if (b == 16 || b == 8) {
                *--s = '0';
            }
        }
        if (sign) {
            *--s = sign;
        }
    }

    return pc + prints(out, out_len, s, width, flags);
}

static int print(char **out, u32 *out_len, const char *format, va_list args)
{
    bool flags_done;
    int width, flags, pc = 0;
    char type, scr[2], *tout;
    bool use_tbuf = (!out) ? true : false;

    if (use_tbuf) {
        console_tbuf_len = CONSOLE_TBUF_MAX;
        tout = console_tbuf;
        out = &tout;
        out_len = &console_tbuf_len;
    }

    if (out) {
        if(!out_len || *out_len) {
            **out = '\0';
        }
    }

    for (; *format != 0; ++format) {
        width = flags = 0;
        if (use_tbuf) {
            flags |= USE_TBUF;
        }
        if (*format == '%') {
            ++format;
            if (*format == '\0') {
                break;
            }
            if (*format == '%') {
                goto literal;
            }
            flags_done = false;
            while (!flags_done) {
                switch (*format) {
                case '-':
                    flags |= PAD_RIGHT;
                    break;
                case '+':
                    flags |= PAD_SIGN;
                    break;
                case '#':
                    flags |= PAD_ALTERNATE;
                    break;
                case '0':
                    flags |= PAD_ZERO;
                    break;
                case ' ':
                case '\'':
                    break;
                default:
                    flags_done = true;
                    break;
                }
                if (!flags_done) {
                    ++format;
                }
            }
            if (flags & PAD_RIGHT) {
                flags &= ~PAD_ZERO;
            }
            for (; *format >= '0' && *format <= '9'; ++format) {
                width *= 10;
                width += *format - '0';
            }
            if (*format == 's') {
                char *s = va_arg(args, char *);
                pc += prints(out, out_len, s ? s : "(null)", width, flags);
                continue;
            }
            if ((*format == 'd') || (*format == 'i')) {
                pc += printi(out, out_len, va_arg(args, int), width, flags, *format);
                continue;
            }
            if ((*format == 'u') || (*format == 'o') || (*format == 'x') || (*format == 'X')) {
                pc += printi(out, out_len, va_arg(args, unsigned int), width, flags, *format);
                continue;
            }
            if ((*format == 'p') || (*format == 'P')) {
                pc += printi(out, out_len, (uintptr_t)va_arg(args, void*), width, flags, *format);
                continue;
            }
            if (*format == 'l') {
                type = 'i';
                if (format[1] == 'l') {
                    ++format;
                    if ((format[1] == 'u') || (format[1] == 'o') ||
                        (format[1] == 'd') || (format[1] == 'i') || 
                        (format[1] == 'x') || (format[1] == 'X'))
                    {
                        ++format;
                        type = *format;
                    }
                    pc += printi(out, out_len, va_arg(args, long long), width, flags, type);
                    continue;
                }
                if ((format[1] == 'u') || (format[1] == 'o') || 
                    (format[1] == 'd') || (format[1] == 'i') || 
                    (format[1] == 'x') || (format[1] == 'X'))
                {
                    ++format;
                    type = *format;
                }
                if ((type == 'd') || (type == 'i')) {
                    pc += printi(out, out_len, va_arg(args, long), width, flags, type);
                }
                else {
                    pc += printi(out, out_len, va_arg(args, unsigned long), width, flags, type);
                }
                continue;
            }
            if (*format == 'c') {
                scr[0] = va_arg(args, int);
                scr[1] = '\0';
                pc += prints(out, out_len, scr, width, flags);
                continue;
            }
        } 
        else {
literal:
            printc(out, out_len, *format, flags);
            ++pc;
        }
    }


    if (use_tbuf && console_tbuf_len < CONSOLE_TBUF_MAX) {
        nputs_all(console_tbuf, CONSOLE_TBUF_MAX - console_tbuf_len);
    }
    return pc;
}

int printf(const char *format, ...)
{
    va_list args;
    int retval = 0;

    va_start(args, format);
    retval = print(NULL, NULL, format, args);
    va_end(args);
    return retval;
}

void __memset(void *p, u8 val, u32 len)
{
    int cc = 0;
    u8 *src = p;

    for (cc=0; cc<len; cc++) {
        src[cc] = val;
    }
}

void __memcpy(void *d, void *s, u32 len)
{
    int cc = 0;
    u8 *src = s;
    u8 *dst = d;

    for (cc=0; cc<len; cc++) {
        dst[cc] = src[cc];
    }
}

void strcpy(char *d, const char *s)
{
    int cc = 0;

    for (cc=0; s[cc]; cc++) {
        d[cc] = s[cc];
    }
}
