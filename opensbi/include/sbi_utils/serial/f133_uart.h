/* SPDX-License-Identifier: BSD-2-Clause */

#ifndef __SERIAL_F133_UART_H__
#define __SERIAL_F133_UART_H__

int f133_uart_init(void);
int f133_console_init(void);

void f133_uart_u8(u8 v);
void f133_uart_u32(u32 v);
void f133_uart_putc(char ch);
void f133_uart_puts(const char *p);

#endif
