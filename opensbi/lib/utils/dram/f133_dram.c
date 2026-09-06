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
#include <sbi_utils/dram/f133.h>
#include <sbi_utils/clock/f133.h>
#include <sbi_utils/common/f133.h>
#include <sbi_utils/serial/f133_uart.h>
#include <sbi_utils/display/f133_slcd.h>

dram_para_t para = {
    528,        // clock
    2,          // ddr2
    0x007b7bf9, // zq
    0x00000000, // odt
    0x000000d2, // para1
    0x00000000, // para2
    0x00000e73, // mr0
    0x00000002, // mr1
    0x00000000, // mr2
    0x00000000, // mr3
    0x00471992, // tpr0
    0x0131a10c, // tpr1
    0x00057041, // tpr2
    0xb4787896, // tpr3
    0x00000000, // tpr4
    0x48484848, // tpr5
    0x00000048, // tpr6
    0x1621121e, // tpr7
    0x00000000, // tpr8
    0x00000000, // tpr9
    0x00000000, // tpr10
    0x00030010, // tpr11
    0x00000035, // tpr12
    0x34000000, // tpr13
    0, 0, 0, 0, 0, 0, 0, 0
};

void f133_dram_init(void)
{
    init_DRAM(0, &para);
}
