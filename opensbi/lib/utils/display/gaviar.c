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
#include <sbi_utils/gpio/f133.h>
#include <sbi_utils/common/f133.h>
#include <sbi_utils/serial/f133_uart.h>
#include <sbi_utils/display/f133_slcd.h>

#include "logo.h"
#include "font8x8.h"

#define PD_CFG0    0x0090
#define PD_CFG1    0x0094
#define PD_CFG2    0x0098
#define PD_DAT     0x00a0

#define LCD_RST    (1 << 0)
#define LCD_WR     (1 << 18)
#define LCD_RS     (1 << 19)
#define LCD_RD     (1 << 20)
#define LCD_CS     (1 << 21)
#define LCD_BL     (1 << 22)

#define LCD_W      320
#define LCD_H      240
#define BG_COLOR   0x000f
#define LINE_MAX   16
#define LINE_LEN   40
#define LINE_START 80

static int log_pos = 0;
static char log_buf[LINE_MAX][LINE_LEN] = {0};

static void send(unsigned int v, unsigned int f)
{
    volatile unsigned int t0, t1;

    t0 = (v & 0x00ff) << 1;
    t1 = (v & 0xff00) << 2;
    writel(t0 | t1 | f, SUNXI_PIO_BASE + PD_DAT);
    writel(t0 | t1 | f | LCD_WR, SUNXI_PIO_BASE + PD_DAT);
}

static void dat(unsigned int v)
{
    send(v, LCD_RS | LCD_RD | LCD_BL | LCD_RST);
}

static void cmd(unsigned int v)
{
    send(v, LCD_RD | LCD_BL | LCD_RST);
}

static void reset(void)
{
    writel(0x00000000, SUNXI_PIO_BASE + PD_DAT);
    udelay(250000);
    writel(0xffffffff, SUNXI_PIO_BASE + PD_DAT);
    udelay(250000);
}

static void setxy(int x, int y)
{
    cmd(0x2b);
    dat(y >> 8);
    dat(y & 0xff);
    dat(LCD_H >> 8);
    dat(LCD_H & 0xff);

    cmd(0x2a);
    dat(x >> 8);
    dat(x & 0xff);
    dat(LCD_W >> 8);
    dat(LCD_W & 0xff);
        
    cmd(0x2c);
}

static void draw_char(int x, int y, char ch, unsigned int fg, unsigned int bg)
{
    int cy = 0;
    unsigned char d = 0;

    for (cy = 0; cy < 8; cy++) {
        setxy(x, y + cy);
        d = font8x8[(ch * 8) + cy];
        dat(d & 0x80 ? fg : bg); 
        dat(d & 0x40 ? fg : bg); 
        dat(d & 0x20 ? fg : bg); 
        dat(d & 0x10 ? fg : bg); 
        dat(d & 0x08 ? fg : bg); 
        dat(d & 0x04 ? fg : bg); 
        dat(d & 0x02 ? fg : bg); 
        dat(d & 0x01 ? fg : bg); 
    }
}

static void draw_text(int x, int y, const char *p, unsigned int fg, unsigned int bg)
{
    int idx = 0, cy = 0;
    unsigned char d = 0;

    for (idx = 0; p[idx]; idx++){
        for (cy = 0; cy < 8; cy++) {
            setxy(x + (idx * 8), y + cy);
            d = font8x8[(p[idx] * 8) + cy];
            dat(d & 0x80 ? fg : bg); 
            dat(d & 0x40 ? fg : bg); 
            dat(d & 0x20 ? fg : bg); 
            dat(d & 0x10 ? fg : bg); 
            dat(d & 0x08 ? fg : bg); 
            dat(d & 0x04 ? fg : bg); 
            dat(d & 0x02 ? fg : bg); 
            dat(d & 0x01 ? fg : bg); 
        }
    }
}

static void clear_bg(void)
{
    int x = 0, y = 0;

    for (y = 0; y < LCD_H; y++) {
        for (x = 0; x < LCD_W; x++) {
            dat(BG_COLOR);
        }
    }
}

static void show_logo(void)
{
    const int W = 100;
    const int H = 80;

    int x = 0, y = 0;
    unsigned short *p = logo;

    for (y = 0; y < H; y++) {
        setxy(0, y);
        for (x = 0; x < W; x++) {
            dat(*p++);
        }
    }
}

static void show_info(void)
{
    draw_text(110, 10, "Gaviar Handheld",         0xffe0, BG_COLOR);
    draw_text(110, 20, "by bankbank and Steward", 0xffe0, BG_COLOR);
    draw_text(110, 30, "09/06/2026",              0xffe0, BG_COLOR);
    draw_text(110, 60, "CPU: Allwinner F133",     0x07e0, BG_COLOR);
    draw_text(110, 70, "LCD: ST7789",             0x07e0, BG_COLOR);
}

static void clear_log(void)
{
    int x = 0, y = 0;

    setxy(0, LINE_START);
    for (y = 0; y < (LCD_H - LINE_START); y++) {
        for (x = 0; x < LCD_W; x++) {
            dat(0x0000);
        }
    }
}

void f133_slcd_putc(char ch)
{
    int c0 = 0, c1 = 0;

    if (((ch < 0x20) || (ch > 0x7e)) && (log_pos > 0)){
        for (c0 = 0; c0 < (LINE_MAX - 1); c0++) {
            __memcpy(log_buf[c0], log_buf[c0 + 1], LINE_LEN);
        }
        __memset(log_buf[LINE_MAX - 1], ' ', LINE_LEN);
            
        for (c0 = 0; c0 < LINE_MAX; c0++) {
            for (c1 = 0; c1 < LINE_LEN; c1++) {
                draw_char(c1 * 8, LINE_START + (c0 * 10), log_buf[c0][c1], 0x7bef, 0x0000);
            }
        }
        log_pos = 0;
    }
    else if ((ch >= 0x20) && (ch <= 0x7e) && (log_pos < LINE_LEN)) {
        draw_char(log_pos * 8, LINE_START + ((LINE_MAX - 1) * 10), ch, 0x7bef, 0x0000);
        log_buf[LINE_MAX - 1][log_pos] = ch;
        log_pos+= 1;
    }
}

void f133_slcd_puts(const char *p)
{
    int cc = 0;

    for (cc=0; p[cc]; cc++) {
        f133_slcd_putc(p[cc]);
    }
}

void f133_slcd_init(void)
{
    writel(0x11111111, SUNXI_PIO_BASE + PD_CFG0);
    writel(0x11111111, SUNXI_PIO_BASE + PD_CFG1);
    writel(0x11111111, SUNXI_PIO_BASE + PD_CFG2);
    writel(0xffffffff, SUNXI_PIO_BASE + PD_DAT);

    reset();
    
    cmd(0xb2);
    dat(0x5c);
    dat(0x5c);
    dat(0x00);
    dat(0x33);
    dat(0x33);

    cmd(0xb7);
    dat(0x35);

    cmd(0x21);
    cmd(0x11);

    udelay(250000);

    cmd(0xe0);
    dat(0xd0);
    dat(0x06);
    dat(0x0b);
    dat(0x07);
    dat(0x07);
    dat(0x24);
    dat(0x2e);
    dat(0x32);
    dat(0x46);
    dat(0x37);
    dat(0x13);
    dat(0x13);
    dat(0x2d);
    dat(0x33);

    cmd(0xe1);
    dat(0xd0);
    dat(0x02);
    dat(0x06);
    dat(0x09);
    dat(0x08);
    dat(0x05);
    dat(0x29);
    dat(0x44);
    dat(0x42);
    dat(0x38);
    dat(0x14);
    dat(0x14);
    dat(0x2a);
    dat(0x30);

    cmd(0x36);
    dat(0xb0);

    cmd(0x2a);
    dat(0x00);
    dat(0x00);
    dat(0x01);
    dat(0x3f);

    cmd(0x2b);
    dat(0x00);
    dat(0x00);
    dat(0x00);
    dat(0xef);

    cmd(0x3a);
    dat(0x55);

    cmd(0x2c);

    clear_bg();
    show_logo();
    show_info();
    clear_log();
    
    cmd(0x29);
}
