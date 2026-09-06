/* SPDX-License-Identifier: BSD-2-Clause */

#ifndef __GPIO_F133_H__
#define __GPIO_F133_H__

#define SUNXI_PIO_BASE     0x02000000
#define GPIO_POW_MODE_REG  0x0340

typedef struct _normal_gpio_cfg {
    char      port;            /* port : PA/PB/PC ... */
    char      port_num;        /* internal port num: PA00/PA01 ... */
    char      mul_sel;         /* function num: input/output/io-disalbe ...*/
    char      pull;            /* pull-up/pull-down/no-pull */
    char      drv_level;       /* driver level: level0-3*/
    char      data;            /* pin state when the port is configured as input or output*/
    char      reserved[2];
}normal_gpio_cfg;

#endif
