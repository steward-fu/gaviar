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

#ifdef BSP_DRV_UART
#include <rthw.h>
#include <rtdevice.h>
#include "drv_uart.h"
#include "board.h"

struct hw_uart_device
{
    rt_uint32_t hw_base;
    rt_uint32_t irqno;

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

/* I/O port base addr */
#define SERIAL0_BASE   0X3F8
#define SERIAL1_BASE   0X2F8

#define SERIAL0_IRQ    4
#define SERIAL1_IRQ    3

#define MAX_BAUD_VALUE  115200
#define DEFAULT_BAUD_VALUE  115200
#define DEFAULT_DIVISOR_VALUE (MAX_BAUD_VALUE / DEFAULT_BAUD_VALUE)

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

enum uart_line_status_register_bits
{
    LINE_STATUS_DATA_READY                  = 1,        /* Data Ready */
    LINE_STATUS_OVERRUN_ERROR               = (1 << 1), /* Overrun Error */
    LINE_STATUS_PARITY_ERROR                = (1 << 2), /* Parity Error */
    LINE_STATUS_FRAMING_ERROR               = (1 << 3), /* Framing Error */
    LINE_STATUS_BREAK_INTERRUPT             = (1 << 4), /* Break Interrupt */
    LINE_STATUS_EMPTY_TRANSMITTER_HOLDING   = (1 << 5), /* Empty Transmitter Holding Register */
    LINE_STATUS_EMPTY_DATA_HOLDING          = (1 << 6), /* Empty Data Holding Registers */
    LINE_STATUS_ERROR_RECEIVE_FIFO          = (1 << 7), /* Error in Received FIFO */
};

enum uart_intr_indenty_reg_bits
{
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

enum uart_modem_control_register_bits
{
    MCR_DTR     = 1,        /* Programs -DTR. If set, -DTR is low and the DTR pin of the port goes 'high'. */
    MCR_RTS     = (1 << 1), /* Programs -RTS. dito.  */
    MCR_OUT1    = (1 << 2), /* Programs -OUT1. Normally not used in a PC, but used with some
                            multi-port serial adapters to enable or disable a port. Best
                            thing is to write a '1' to this bit. */
    MCR_OUT2    = (1 << 3), /* Programs -OUT2. If set to 1, interrupts generated by the UART
                            are transferred to the ICU (Interrupt Control Unit) while 0
                            sets the interrupt output of the card to high impedance.
                            (This is PC-only). */
    MCR_LOOPBACK    = (1 << 4),  /* '1': local loopback. All outputs disabled. This is a means of
                            testing the chip: you 'receive' all the data you send. */
};

static void rt_hw_uart_isr(int irqno, void *param)
{
    struct rt_serial_device *serial = (struct rt_serial_device *)param;
    rt_hw_serial_isr(serial, RT_SERIAL_EVENT_RX_IND);
}

static rt_err_t uart_configure(struct rt_serial_device *serial, struct serial_configure *cfg)
{
    RT_ASSERT(serial != RT_NULL);
    serial->config = *cfg;
    return RT_EOK;
}

static rt_err_t uart_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    struct hw_uart_device *uart;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct hw_uart_device *)serial->parent.user_data;
    rt_uint8_t val;
    switch (cmd)
    {
    case RT_DEVICE_CTRL_CLR_INT:
        /* disable rx irq */
        val = inb(uart->intr_enable_reg);
        outb(uart->intr_enable_reg, val & ~INTR_RECV_DATA_AVALIABLE);
        break;

    case RT_DEVICE_CTRL_SET_INT:
        /* enable rx irq */
        val = inb(uart->intr_enable_reg);
        outb(uart->intr_enable_reg, val | INTR_RECV_DATA_AVALIABLE);
        break;
    }
    return RT_EOK;
}

static int uart_putc(struct rt_serial_device *serial, char c)
{
    struct hw_uart_device *uart;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct hw_uart_device *)serial->parent.user_data;

    int timeout = 100000;
    while (!(inb(uart->line_status_reg) & LINE_STATUS_EMPTY_TRANSMITTER_HOLDING) && timeout--)
    {
    }
    outb(uart->data_reg, c);
    return 1;
}

static int uart_getc(struct rt_serial_device *serial)
{
    struct hw_uart_device *uart;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct hw_uart_device *)serial->parent.user_data;

    int timeout = 100000;
    while (!(inb(uart->line_status_reg) & LINE_STATUS_DATA_READY) && timeout--)
    {
    }
    int data = -1;
    if (timeout > 0)
    {
        data = inb(uart->data_reg);
    }
    return data;
}

static const struct rt_uart_ops _uart_ops =
{
    uart_configure,
    uart_control,
    uart_putc,
    uart_getc,
};

#ifdef RT_USING_UART0
/* UART device driver structure */
static struct hw_uart_device _uart0_device =
{
    SERIAL0_BASE,
    SERIAL0_IRQ,
};
static struct rt_serial_device _serial0;
#endif  /* RT_USING_UART0 */

#ifdef RT_USING_UART1
/* UART1 device driver structure */
static struct hw_uart_device _uart1_device =
{
    SERIAL1_BASE,
    SERIAL1_IRQ,
};
static struct rt_serial_device _serial1;
#endif  /* RT_USING_UART1 */

#if defined(RT_USING_UART0) || defined(RT_USING_UART1)
static void do_uart_init(char *name, struct hw_uart_device *uart, struct rt_serial_device *serial)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    rt_uint32_t iobase = uart->hw_base;

    uart->data_reg         = iobase + 0;
    uart->divisor_low_reg  = iobase + 0;
    uart->intr_enable_reg  = iobase + 1;
    uart->divisor_high_reg = iobase + 1;
    uart->intr_indenty_reg = iobase + 2;
    uart->line_ctrl_reg    = iobase + 3;
    uart->modem_ctrl_reg   = iobase + 4;
    uart->line_status_reg  = iobase + 5;
    uart->modem_status_reg = iobase + 6;
    uart->scratch_reg      = iobase + 7;

    /* Setting can change the baud rate Baud */
    outb(uart->line_ctrl_reg, LINE_DLAB);

    /* Set Baud rate */
    outb(uart->divisor_low_reg, (MAX_BAUD_VALUE / config.baud_rate) & 0xff);
    outb(uart->divisor_high_reg, ((MAX_BAUD_VALUE / config.baud_rate) >> 8) & 0xff);

    /* Set DLAB to 0, set the character width to 8, stop bit to 1, no parity, break signal Disabled */
    outb(uart->line_ctrl_reg, LINE_WORD_LENGTH_8 |
        LINE_STOP_BIT_1 | LINE_PARITY_NO);

    /* enable recv intr */
    outb(uart->intr_enable_reg, INTR_RECV_DATA_AVALIABLE |
        INTR_STATUS_CHANGED | INTR_LOW_POWER_MODE);

    /*
     * Set FIFO, open FIFO, clear receive FIFO, clear transmit FIFO Open 64Byte FIFO,
     * interrupt trigger level is 14Byte
     */
    outb(uart->fifo_reg, FIFO_ENABLE | FIFO_CLEAR_TRANSMIT |
        FIFO_CLEAR_RECEIVE | FIFO_ENABLE_64 |
        FIFO_TRIGGER_14);

    /* IRQs enabled, RTS/DSR set */
    outb(uart->modem_ctrl_reg, MCR_DTR | MCR_RTS | MCR_OUT2);
    outb(uart->scratch_reg, 0x00);

    serial->ops    = &_uart_ops;
    serial->config = config;

    /* register device */
    rt_hw_serial_register(serial, name,
                          RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_STREAM,
                          uart);
    rt_hw_interrupt_install(uart->irqno, rt_hw_uart_isr, serial, name);
    rt_hw_interrupt_umask(uart->irqno);
}
#endif

int rt_hw_uart_init(void)
{
#ifdef RT_USING_UART0
    do_uart_init("uart0", &_uart0_device, &_serial0);
#endif  /* RT_USING_UART0 */

#ifdef RT_USING_UART1
    do_uart_init("uart1", &_uart1_device, &_serial1);
#endif  /* RT_USING_UART1 */
    return 0;
}
#endif  /* BSP_DRV_UART */
