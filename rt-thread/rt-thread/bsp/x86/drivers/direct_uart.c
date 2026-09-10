/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2021-07-15     JasonHu       first version
 */

#include <rtconfig.h>

#ifdef BSP_USING_DIRECT_UART
#include <rthw.h>
#include <rtdevice.h>

#include "board.h"

/* I/O port base addr */
#define COM1_BASE   0X3F8

#define MAX_BAUD_VALUE  115200
#define DEFAULT_BAUD_VALUE  19200
#define DEFAULT_DIVISOR_VALUE (MAX_BAUD_VALUE / DEFAULT_BAUD_VALUE)

#define UART_SEND_TIMEOUT

enum uart_fifo_control_register_bits
{
    FIFO_ENABLE = 1,                             /* Enable FIFOs */
    FIFO_CLEAR_RECEIVE   = (1 << 1),             /* Clear Receive FIFO */
    FIFO_CLEAR_TRANSMIT  = (1 << 2),             /* Clear Transmit FIFO */
    FIFO_DMA_MODE_SELECT = (1 << 3),             /* DMA Mode Select */
    FIFO_RESERVED        = (1 << 4),             /* Reserved */
    FIFO_ENABLE_64       = (1 << 5),             /* Enable 64 Byte FIFO(16750) */
    /* Interrupt Trigger Level/Trigger Level  */
    FIFO_TRIGGER_1       = (0 << 6),             /* 1 Byte */
    FIFO_TRIGGER_4       = (1 << 6),             /* 4 Byte */
    FIFO_TRIGGER_8       = (1 << 7),             /* 8 Byte */
    FIFO_TRIGGER_14      = (1 << 6) | (1 << 7),  /* 14 Byte */
};

enum uart_line_control_register_bits
{
    /* Word Length */
    LINE_WORD_LENGTH_5   = 0,                    /* 5 Bits */
    LINE_WORD_LENGTH_6   = 1,                    /* 6 Bits */
    LINE_WORD_LENGTH_7   = (1 << 1),             /* 7 Bits */
    LINE_WORD_LENGTH_8   = ((1 << 1) | 1),       /* 8 Bits */
    LINE_STOP_BIT_1      = (0 << 2),             /* One Stop Bit */
    LINE_STOP_BIT_2      = (1 << 2),             /* 1.5 Stop Bits or 2 Stop Bits */
        /* Parity Select */
    LINE_PARITY_NO       = (0 << 3),             /* No Parity */
    LINE_PARITY_ODD      = (1 << 3),             /* Odd Parity */
    LINE_PARITY_EVEN     = (1 << 3) | (1 << 4),  /* Even Parity */
    LINE_PARITY_MARK     = (1 << 3) | (1 << 5),  /* Mark */
    LINE_PARITY_SPACE    = (1 << 3) | (1 << 4) | (1 << 5), /* Space */
    LINE_BREAK_ENABLE    = (1 << 6),             /* Set Break Enable */
    LINE_DLAB            = (1 << 7),             /* Divisor Latch Access Bit */
};

enum uart_interrupt_enable_register_bits
{
    INTR_RECV_DATA_AVALIABLE = 1,        /* Enable Received Data Available Interrupt */
    INTR_TRANSMIT_HOLDING    = (1 << 1), /* Enable Transmitter Holding Register Empty Interrupt */
    INTR_STATUS_CHANGED      = (1 << 2), /* Enable Receiver Line Status Interrupt */
    INTR_MODEM_STATUS        = (1 << 3), /* Enable Modem Status Interrupt */
    INTR_SLEEP_MODE          = (1 << 4), /* Enable Sleep Mode(16750) */
    INTR_LOW_POWER_MODE      = (1 << 5), /* Enable Low Power Mode(16750) */
    INTR_RESERVED1           = (1 << 6), /* Reserved */
    INTR_RESERVED2           = (1 << 7), /* Reserved */
};

enum uart_line_status_register_bits {
    LINE_STATUS_DATA_READY                  = 1,        /* Data Ready */
    LINE_STATUS_OVERRUN_ERROR               = (1 << 1), /* Overrun Error */
    LINE_STATUS_PARITY_ERROR                = (1 << 2), /* Parity Error */
    LINE_STATUS_FRAMING_ERROR               = (1 << 3), /* Framing Error */
    LINE_STATUS_BREAK_INTERRUPT             = (1 << 4), /* Break Interrupt */
    LINE_STATUS_EMPTY_TRANSMITTER_HOLDING   = (1 << 5), /* Empty Transmitter Holding Register */
    LINE_STATUS_EMPTY_DATA_HOLDING          = (1 << 6), /* Empty Data Holding Registers */
    LINE_STATUS_ERROR_RECEIVE_FIFO          = (1 << 7), /* Error in Received FIFO */
};

enum uart_intr_indenty_reg_bits {
    INTR_STATUS_PENDING_FLAG        = 1,        /* Interrupt Pending Flag */
    /* 产生的什么中断 */
    INTR_STATUS_MODEM               = (0 << 1), /* Transmitter Holding Register Empty Interrupt  */
    INTR_STATUS_TRANSMITTER_HOLDING = (1 << 1), /* Received Data Available Interrupt */
    INTR_STATUS_RECEIVE_DATA        = (1 << 2), /* Received Data Available Interrupt */
    INTR_STATUS_RECEIVE_LINE        = (1 << 1) | (1 << 2),  /* Receiver Line Status Interrupt */
    INTR_STATUS_TIME_OUT_PENDING    = (1 << 2) | (1 << 3),  /* Time-out Interrupt Pending (16550 & later) */
    INTR_STATUS_64BYTE_FIFO         = (1 << 5), /* 64 Byte FIFO Enabled (16750 only) */
    INTR_STATUS_NO_FIFO             = (0 << 6), /* No FIFO on chip */
    INTR_STATUS_RESERVED_CONDITION  = (1 << 6), /* Reserved condition */
    INTR_STATUS_FIFO_NOT_FUNC       = (1 << 7), /* FIFO enabled, but not functioning */
    INTR_STATUS_FIFO                = (1 << 6) | (1 << 7),  /* FIFO enabled */
};

struct hw_uart
{
    rt_uint16_t iobase;
    rt_uint16_t data_reg;
    rt_uint16_t divisor_low_reg;
    rt_uint16_t intr_enable_reg;
    rt_uint16_t divisor_high_reg;
    rt_uint16_t intr_indenty_reg;
    rt_uint16_t fifo_reg;
    rt_uint16_t line_ctrl_reg;
    rt_uint16_t modem_ctrl_reg;
    rt_uint16_t line_status_reg;
    rt_uint16_t modem_status_reg;
    rt_uint16_t scratch_reg;
};
typedef struct hw_uart hw_uart_t;

static hw_uart_t hw_uart;

static int uart_send(hw_uart_t *uart, char data)
{
#ifdef UART_SEND_TIMEOUT
    int timeout = 0x100000;
    while (!(inb(uart->line_status_reg) & LINE_STATUS_EMPTY_TRANSMITTER_HOLDING) && timeout--)
    {
    }
#else
    while (!(inb(uart->line_status_reg) & LINE_STATUS_EMPTY_TRANSMITTER_HOLDING))
    {
    }
#endif
    outb(uart->data_reg, data);
    return 0;
}

void rt_hw_direct_uart_putchar(char ch)
{
    if(ch == '\n') {
        uart_send(&hw_uart, '\r');
    }
    uart_send(&hw_uart, ch);
}

/**
* This function is used by rt_kprintf to display a string on console.
*
* @param str the displayed string
*/
void rt_hw_console_output(const char *str)
{
    while (*str) {
        rt_hw_direct_uart_putchar(*str++);
    }
}

void rt_hw_direct_uart_init(void)
{
    hw_uart_t *uart = &hw_uart;
    rt_uint16_t iobase;

    iobase = COM1_BASE;
    uart->iobase                         = iobase;
    uart->data_reg                       = iobase + 0;
    uart->divisor_low_reg                = iobase + 0;
    uart->intr_enable_reg                = iobase + 1;
    uart->divisor_high_reg               = iobase + 1;
    uart->intr_indenty_reg               = iobase + 2;
    uart->line_ctrl_reg                  = iobase + 3;
    uart->modem_ctrl_reg                 = iobase + 4;
    uart->line_status_reg                = iobase + 5;
    uart->modem_status_reg               = iobase + 6;
    uart->scratch_reg                    = iobase + 7;

    outb(uart->line_ctrl_reg, LINE_DLAB);

    outb(uart->divisor_low_reg, (DEFAULT_DIVISOR_VALUE) & 0xff);
    outb(uart->divisor_high_reg, ((DEFAULT_DIVISOR_VALUE) >> 8) & 0xff);

    outb(uart->line_ctrl_reg, LINE_WORD_LENGTH_8 |
        LINE_STOP_BIT_1 | LINE_PARITY_NO);

    /* close all intr */
    outb(uart->intr_enable_reg, 0);

    outb(uart->fifo_reg, FIFO_ENABLE | FIFO_CLEAR_TRANSMIT |
        FIFO_CLEAR_RECEIVE | FIFO_ENABLE_64 |
        FIFO_TRIGGER_14);

    outb(uart->modem_ctrl_reg, 0x00);
    outb(uart->scratch_reg, 0x00);
}
#endif  /* BSP_USING_DIRECT_UART */
