/* SPDX-License-Identifier: BSD-2-Clause */

#ifndef __DRAM_F133_H__
#define __DRAM_F133_H__

typedef struct __DRAM_PARA {
    // normal configuration
    u32 dram_clk;

    // dram_type
    //   DDR2: 2
    //   DDR3: 3
    //   LPDDR2: 6
    //   LPDDR3: 7
    //   DDR3L: 31
    u32 dram_type;

    u32 dram_zq;
    u32 dram_odt_en;

    // control configuration
    u32 dram_para1;
    u32 dram_para2;

    // timing configuration
    u32 dram_mr0;
    u32 dram_mr1;
    u32 dram_mr2;
    u32 dram_mr3;
    u32 dram_tpr0;
    u32 dram_tpr1;
    u32 dram_tpr2;
    u32 dram_tpr3;
    u32 dram_tpr4;
    u32 dram_tpr5;
    u32 dram_tpr6;
    u32 dram_tpr7;
    u32 dram_tpr8;
    u32 dram_tpr9;
    u32 dram_tpr10;
    u32 dram_tpr11;
    u32 dram_tpr12;
    u32 dram_tpr13;

    u32 dummy0;
    u32 dummy1;
    u32 dummy2;
    u32 dummy3;
    u32 dummy4;
    u32 dummy5;
    u32 dummy6;
    u32 dummy7;
} dram_para_t;

void f133_dram_init(void);
u32 init_DRAM(int type, dram_para_t *para);

#endif

