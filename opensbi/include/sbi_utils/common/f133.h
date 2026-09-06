/* SPDX-License-Identifier: BSD-2-Clause */

#ifndef __COMMON_F133_H__
#define __COMMON_F133_H__

void clrbits(u32 addr, u32 bits);
void setbits(u32 addr, u32 bits);
void clrsetbits(u32 addr, u32 clr, u32 set);
u32 get_sys_ticks(void);
u32 timer_get_us(void);
void udelay(u32 us);
void mdelay(u32 ms);
void __usdelay(u32 us);
void __msdelay(u32 ms);
int printf(const char *format, ...);
void __memset(void *p, u8 val, u32 len);
void __memcpy(void *dst, void *src, u32 len);
void strcpy(char *d, const char *s);

#endif

