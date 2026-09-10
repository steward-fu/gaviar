/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-07-14     JasonHu      first version
 */

#ifndef __BOARD_H__
#define __BOARD_H__

#include <rtconfig.h>
#include <interrupt.h>
#include <cpuport.h>

/* boot size */
#define HW_KERNEL_BEGIN     0x00000000UL
#define HW_KERNEL_SIZE      (8 * 1024 * 1024)

/* DMA start at 8M (DMA must lower than 16 MB address) */
#define HW_DMA_BEGIN      (HW_KERNEL_BEGIN + HW_KERNEL_SIZE)
/* DMA 8 MB size */
#define HW_DMA_SIZE       (8 * 1024 * 1024)

/* heap start at 16M */
#define HW_HEAP_BEGIN      (HW_DMA_BEGIN + HW_DMA_SIZE)
/* heap 16 MB size */
#define HW_HEAP_SIZE       (16 * 1024 * 1024)

#define HW_KERNEL_DELAY_MAP_MB    128

#ifdef RT_USING_USERSPACE
#define HW_HEAP_END        (void*)(KERNEL_VADDR_START + HW_HEAP_BEGIN + HW_HEAP_SIZE)

#define HW_PAGE_START      HW_HEAP_END
/* TODO: use dynamic memroy select size */
#define HW_PAGE_SIZE_MIN   ((64 - 32) * 1024 * 1024)
#define HW_PAGE_SIZE_MAX   (((1024 - HW_KERNEL_DELAY_MAP_MB) - 32) * 1024 * 1024)

#define HW_PAGE_SIZE_DEF   ((256 - 32) * 1024 * 1024)

/* this should at end of phy memory */
#define HW_PAGE_END        (void*)(HW_PAGE_START + HW_PAGE_SIZE_DEF)

#define HW_PHY_MEM_SIZE_MIN (HW_KERNEL_SIZE + HW_DMA_SIZE + HW_HEAP_SIZE + HW_PAGE_SIZE_MIN)

#else
#define HW_HEAP_END        (void*)(HEAP_BEGIN + HW_HEAP_SIZE)
#endif

/* start at 1G, end at 4G */
#define HW_USER_START   0x40000000UL
#define HW_USER_END     0xFFFFF000UL
#define HW_USER_SIZE    (HW_USER_END - HW_USER_START)

/*
 * Delay map, don't map when kernel do self map, only map when needed.
 * This area was used ioremap.
 */
#define HW_KERNEL_DELAY_MAP_SIZE    (HW_KERNEL_DELAY_MAP_MB * (1024 * 1024))
#define HW_KERNEL_START             (0)
#define HW_KERNEL_END               (HW_USER_START - HW_KERNEL_DELAY_MAP_SIZE)
#define HW_KERNEL_DELAY_MAP_START   HW_KERNEL_END

/**
 * Virtual memory layout:
 *
 * +------------+ <- 0xFFFFFFFF (4GB)
 * | USER       |
 * +------------+ <- 0x40000000 (1GB)
 * | DELAY MAP  |
 * +------------+ <- 0x38000000 (896MB)
 * | PAGE       |
 * +------------+ <- 0x02000000 (32MB)
 * | HEAP       |
 * +------------+ <- 0x01000000 (16MB)
 * | DMA        |
 * +------------+ <- 0x00800000 (8MB)
 * | KERNEL     |
 * +------------+ <- 0x00000000
 */

void rt_hw_board_init(void);

#endif
