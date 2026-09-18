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
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/ipi/aclint_mswi.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/serial/sifive-uart.h>
#include <sbi_utils/timer/aclint_mtimer.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/clock/f133.h>
#include <sbi_utils/serial/f133_uart.h>
#include <sbi_utils/display/f133_slcd.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#define F133_HART_COUNT         16
#define F133_PLIC_BASE_ADDR     0x10000000
#define F133_PLIC_CLINT_OFFSET  0x04000000
#define F133_PLIC_NUM_SOURCES   0xb0
#define F133_CLINT_BASE_ADDR    0x14000000
#define F133_ACLINT_MSWI_ADDR   (F133_CLINT_BASE_ADDR + CLINT_MSWI_OFFSET)
#define F133_ACLINT_MTIMER_ADDR (F133_CLINT_BASE_ADDR + CLINT_MTIMER_OFFSET)
#define F133_ACLINT_MTIMER_FREQ 10000000

static struct plic_data plic = {
    .addr = F133_PLIC_BASE_ADDR,
    .num_src = F133_PLIC_NUM_SOURCES,
};

static struct aclint_mswi_data mswi = {
    .addr = F133_ACLINT_MSWI_ADDR,
    .size = ACLINT_MSWI_SIZE,
    .first_hartid = 0,
    .hart_count = F133_HART_COUNT,
};

static struct aclint_mtimer_data mtimer = {
    .mtime_freq = F133_ACLINT_MTIMER_FREQ,
    .mtime_addr = F133_ACLINT_MTIMER_ADDR + ACLINT_DEFAULT_MTIME_OFFSET,
    .mtime_size = ACLINT_DEFAULT_MTIME_SIZE,
    .mtimecmp_addr = F133_ACLINT_MTIMER_ADDR + ACLINT_DEFAULT_MTIMECMP_OFFSET,
    .mtimecmp_size = ACLINT_DEFAULT_MTIMECMP_SIZE,
    .first_hartid = 0,
    .hart_count = F133_HART_COUNT,
    .has_64bit_mmio = false,
};

static int f133_early_init(bool cold_boot)
{
    f133_slcd_init();
    return 0;
}

static int f133_final_init(bool cold_boot)
{
    return 0;
}

static int f133_irqchip_init(bool cold_boot)
{
    int rc = 0;
    u32 hartid = current_hartid();

    if (cold_boot) {
        rc = plic_cold_irqchip_init(&plic);
        if (rc) {
            return rc;
        }
    }

    return plic_warm_irqchip_init(&plic, hartid * 2, hartid * 2 + 1);
}

static int f133_ipi_init(bool cold_boot)
{
    int rc = 0;

    if (cold_boot) {
        rc = aclint_mswi_cold_init(&mswi);
        if (rc) {
            return rc;
        }
    }

    return aclint_mswi_warm_init();
}

static int f133_timer_init(bool cold_boot)
{
    int rc = 0;

    if (cold_boot) {
        rc = aclint_mtimer_cold_init(&mtimer, NULL);
        if (rc) {
            return rc;
        }
    }

    return aclint_mtimer_warm_init();
}

const struct sbi_platform_operations platform_ops = {
    .early_init = f133_early_init,
    .final_init = f133_final_init,
    .console_init = f133_console_init,
    .irqchip_init = f133_irqchip_init,
    .ipi_init = f133_ipi_init,
    .timer_init = f133_timer_init,
};

const struct sbi_platform platform = {
    .opensbi_version = OPENSBI_VERSION,
    .platform_version = SBI_PLATFORM_VERSION(0x0, 0x01),
    .name = "Allwinner F133",
    .features = SBI_PLATFORM_DEFAULT_FEATURES,
    .hart_count = F133_HART_COUNT,
    .hart_stack_size = SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
    .heap_size = SBI_PLATFORM_DEFAULT_HEAP_SIZE(F133_HART_COUNT),
    .platform_ops_addr = (unsigned long)&platform_ops
};

