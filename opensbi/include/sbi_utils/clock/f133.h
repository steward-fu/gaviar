/* SPDX-License-Identifier: BSD-2-Clause */

#ifndef __CLOCK_F133_H__
#define __CLOCK_F133_H__

#define SUNXI_RST_BIT                   16
#define SUNXI_AUDIO_CODEC               0x02030000
#define SUNXI_RTC_BASE                  0x07090000
#define SUNXI_GATING_BIT                0
#define SUNXI_SID_BASE                  0x03006000
#define SUNXI_CCM_BASE                  0x02001000
#define SUNXI_SYSCRL_BASE               0x03000000

#define CCM_UART_RST_OFFSET             16
#define CCM_UART_GATING_OFFSET          0
#define CCMU_PLL_CPUX_CTRL_REG          (SUNXI_CCM_BASE + 0x0)
#define CCMU_PLL_DDR0_CTRL_REG          (SUNXI_CCM_BASE + 0x10)
#define CCMU_PLL_PERI0_CTRL_REG         (SUNXI_CCM_BASE + 0x20)
#define CCMU_PLL_VIDE00_CTRL_REG        (SUNXI_CCM_BASE + 0x40)
#define CCMU_PLL_VIDE01_CTRL_REG        (SUNXI_CCM_BASE + 0x48)
#define CCMU_PLL_VE_CTRL_REG            (SUNXI_CCM_BASE + 0x58)
#define CCMU_PLL_AUDIO0_CTRL_REG        (SUNXI_CCM_BASE + 0x78)
#define CCMU_PLL_AUDIO1_CTRL_REG        (SUNXI_CCM_BASE + 0x80)
#define CCMU_GPADC_BGR_REG              (SUNXI_CCM_BASE + 0x9EC)
#define CCMU_AUDIO_CODEC_BGR_REG        (SUNXI_CCM_BASE + 0xA5C)
#define CCMU_PLL_AUDIO0_PAT0_REG        (SUNXI_CCM_BASE + 0x178)
#define CCMU_CPUX_AXI_CFG_REG           (SUNXI_CCM_BASE + 0xD00)
#define CCMU_PSI_AHB1_AHB2_CFG_REG      (SUNXI_CCM_BASE + 0x510)
#define CCMU_APB1_CFG_GREG              (SUNXI_CCM_BASE + 0x520)
#define CCMU_APB2_CFG_GREG              (SUNXI_CCM_BASE + 0x524)
#define CCMU_MBUS_CFG_REG               (SUNXI_CCM_BASE + 0x540)
#define CCMU_CE_CLK_REG                 (SUNXI_CCM_BASE + 0x680)
#define CCMU_CE_BGR_REG                 (SUNXI_CCM_BASE + 0x68C)
#define CCMU_DMA_BGR_REG                (SUNXI_CCM_BASE + 0x70C)
#define CCMU_AVS_CLK_REG                (SUNXI_CCM_BASE + 0x740)
#define CCMU_DRAM_CLK_REG               (SUNXI_CCM_BASE + 0x800)
#define CCMU_MBUS_MST_CLK_GATING_REG    (SUNXI_CCM_BASE + 0x804)
#define CCMU_DRAM_BGR_REG               (SUNXI_CCM_BASE + 0x80C)
#define CCMU_NAND_CLK_REG               (SUNXI_CCM_BASE + 0x810)
#define CCMU_NAND_BGR_REG               (SUNXI_CCM_BASE + 0x82C)
#define CCMU_SDMMC0_CLK_REG             (SUNXI_CCM_BASE + 0x830)
#define CCMU_SDMMC1_CLK_REG             (SUNXI_CCM_BASE + 0x834)
#define CCMU_SDMMC2_CLK_REG             (SUNXI_CCM_BASE + 0x838)
#define CCMU_SMHC_BGR_REG               (SUNXI_CCM_BASE + 0x84c)
#define CCMU_UART_BGR_REG               (SUNXI_CCM_BASE + 0x90C)
#define CCMU_TWI_BGR_REG                (SUNXI_CCM_BASE + 0x91C)
#define CCMU_SCR_BGR_REG                (SUNXI_CCM_BASE + 0x93C)
#define CCMU_SPI0_CLK_REG               (SUNXI_CCM_BASE + 0x940)
#define CCMU_SPI1_CLK_REG               (SUNXI_CCM_BASE + 0x944)
#define CCMU_SPI_BGR_CLK_REG            (SUNXI_CCM_BASE + 0x96C)
#define CCMU_USB0_CLK_REG               (SUNXI_CCM_BASE + 0xA70)
#define CCMU_USB_BGR_REG                (SUNXI_CCM_BASE + 0xA8C)
#define DMA_GATING_BASE                 CCMU_DMA_BGR_REG
#define DMA_GATING_PASS                 1
#define DMA_GATING_BIT                  0
#define CE_CLK_SRC_MASK                 3
#define CE_CLK_SRC_SEL_BIT              24
#define CE_CLK_SRC                      1
#define CE_CLK_DIV_RATION_N_BIT         8
#define CE_CLK_DIV_RATION_N_MASK        3
#define CE_CLK_DIV_RATION_N             0
#define CE_CLK_DIV_RATION_M_BIT         0
#define CE_CLK_DIV_RATION_M_MASK        0xf
#define CE_CLK_DIV_RATION_M             2
#define CE_SCLK_ONOFF_BIT               31
#define CE_SCLK_ON                      1
#define CE_GATING_BASE                  CCMU_CE_BGR_REG
#define CE_GATING_PASS                  1
#define CE_GATING_BIT                   0
#define CE_RST_REG_BASE                 CCMU_CE_BGR_REG
#define CE_RST_BIT                      16
#define CE_DEASSERT                     1
#define PLL_CPUX_TUNING_REG             0x1400

void f133_clock_init(void);

#endif

