/* 
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (C) 2026 Steward Fu <steward.fu@gmail.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>
#include <sbi_utils/clock/f133.h>
#include <sbi_utils/serial/f133_uart.h>
#include <sbi_utils/display/f133_slcd.h>

#define F133_HART_COUNT 1

static int f133_early_init(bool cold_boot)
{
    f133_slcd_init();
    return 0;
}

static int f133_final_init(bool cold_boot)
{
    return 0;
}

const struct sbi_platform_operations platform_ops = {
    .early_init   = f133_early_init,
    .final_init   = f133_final_init,
    .console_init = f133_console_init
};

const struct sbi_platform platform = {
    .opensbi_version   = OPENSBI_VERSION,
    .platform_version  = SBI_PLATFORM_VERSION(0x0, 0x01),
    .name              = "Allwinner F133",
    .features          = SBI_PLATFORM_DEFAULT_FEATURES,
    .hart_count        = F133_HART_COUNT,
    .hart_stack_size   = SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
    .heap_size         = SBI_PLATFORM_DEFAULT_HEAP_SIZE(F133_HART_COUNT),
    .platform_ops_addr = (unsigned long)&platform_ops
};
