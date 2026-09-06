/* SPDX-License-Identifier: BSD-2-Clause */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>
#include <sbi_utils/gpio/f133.h>
#include <sbi_utils/clock/f133.h>
#include <sbi_utils/common/f133.h>
#include <sbi_utils/display/f133_slcd.h>

#define UART4_BASE      0x02501000

#define PB_CFG0         0x0030
#define UART_BGR_REG    0x090c
 
#define UART_RBR        0x0000
#define UART_THR        0x0000
#define UART_DLL        0x0000
#define UART_DLH        0x0004
#define UART_IER        0x0004
#define UART_IIR        0x0008
#define UART_LCR        0x000c
#define UART_MCR        0x0010
#define UART_LSR        0x0014
#define UART_USR        0x007c

void f133_uart_putc(char ch)
{
    while ((readl(UART4_BASE + UART_LSR) & (1 << 5)) == 0);
    writeb(ch, UART4_BASE);
}

void f133_uart_puts(const char *p)
{
    u32 cc = 0;

    for (cc=0; p[cc]; cc++) {
        f133_uart_putc(p[cc]);
    }
}

static struct sbi_console_device f133_console = {
    .name = "f133_uart",
    .console_putc = f133_uart_putc,
};

void f133_uart_u8(u8 v)
{
    u8 low = v & 0x0f;
    u8 high = (v >> 4) & 0x0f;

    if (high >= 10) {
        f133_uart_putc('a' + (high - 10));
    }
    else {
        f133_uart_putc('0' + high);
    }
    
    if (low >= 10) {
        f133_uart_putc('a' + (low - 10));
    }
    else {
        f133_uart_putc('0' + low);
    }
}

void f133_uart_u32(u32 v)
{
    f133_uart_putc('\r');
    f133_uart_putc('\n');
    f133_uart_putc('0');
    f133_uart_putc('x');
    f133_uart_u8(v >> 24);
    f133_uart_u8(v >> 16);
    f133_uart_u8(v >> 8);
    f133_uart_u8(v >> 0);
}

int f133_uart_init(void)
{
    uint32_t v = 0;

    v = readl(SUNXI_PIO_BASE + PB_CFG0) & 0xffff00ff;
    v|= 0x7700;
    writel(v, SUNXI_PIO_BASE + PB_CFG0);

    v = readl(SUNXI_CCM_BASE + UART_BGR_REG);
    v|= (1 << 20) | (1 << 4);
    writel(v, SUNXI_CCM_BASE + UART_BGR_REG);

    writel(0, UART4_BASE + UART_IER);
    writel(0, UART4_BASE + UART_MCR);

    v = readl(UART4_BASE + UART_LCR);
    v|= (1 << 7);
    writel(v, UART4_BASE + UART_LCR);

    writel(13, UART4_BASE + UART_DLL);
    writel(0, UART4_BASE + UART_DLH);

    v = readl(UART4_BASE + UART_LCR);
    v&= ~(1 << 7);
    writel(v, UART4_BASE + UART_LCR);

    v = readl(UART4_BASE + UART_LCR);
    v&= ~(0x1f);
    v|= 0x03;
    writel(v, UART4_BASE + UART_LCR);

    printf("\n[UART] Gaviar Handheld\n");
    return 0;
}

int f133_console_init(void)
{
    sbi_console_set_device(&f133_console);
    return 0;
}

