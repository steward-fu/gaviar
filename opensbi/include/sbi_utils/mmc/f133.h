/* SPDX-License-Identifier: BSD-2-Clause */

#ifndef __MMC_F133_H__
#define __MMC_F133_H__

#define MMC_REG_BASE                SUNXI_MMC0_BASE
#define SUNXI_SMHC0_BASE            0x04020000
#define SUNXI_SMHC1_BASE            0x04021000
#define SDXC_DES_NUM_SHIFT          12
#define SDXC_DES_BUFFER_MAX_LEN     (1 << SDXC_DES_NUM_SHIFT)
#define DS26_SDR12                  0
#define HSSDR52_SDR25               1
#define HSDDR52_DDR50               2
#define HS200_SDR104                3
#define HS400                       4
#define MAX_SPD_MD_NUM              5
#define CLK_400K                    0
#define CLK_25M                     1
#define CLK_50M                     2
#define CLK_100M                    3
#define CLK_150M                    4
#define CLK_200M                    5
#define MAX_CLK_FREQ_NUM            8
#define SUNXI_MMC_TIMING_MODE_1     1
#define SUNXI_MMC_TIMING_MODE_3     3
#define SUNXI_MMC_TIMING_MODE_4     4
#define SUNXI_MMC0_BASE             SUNXI_SMHC0_BASE
#define SUNXI_MMC1_BASE             SUNXI_SMHC1_BASE
#define SUNXI_MMC2_BASE             SUNXI_SMHC2_BASE
#define MAX_MMC_NUM                 1
#define CCMU_HCLKGATE0_BASE         CCMU_SMHC_BGR_REG
#define CCMU_HCLKRST0_BASE          CCMU_SMHC_BGR_REG
#define CCMU_MMC0_CLK_BASE          CCMU_SDMMC0_CLK_REG
#define CCMU_MMC2_CLK_BASE          CCMU_SDMMC2_CLK_REG
#define WR_MB()                     wmb()
#define DMAC_DES_BASE_IN_SRAM       (0x20000 + 0xC000)
#define DMAC_DES_BASE_IN_SDRAM      0x42000000
#define DRAM_START_ADDR             0x40000000
#define DRIVER_VER                  "2021-04-2 16:45"

#define SUNXI_MMC_1X_2X_MODE_CONTROL_REG        0x03000024
#define IS_SD(x)                                (x->version & SD_VERSION_SD)
#define __mmc_be32_to_cpu(x)                    ((0x000000ff&((x)>>24)) | (0x0000ff00&((x)>>8)) | (0x00ff0000&((x)<<8)) | (0xff000000&((x)<<24)))

struct boot_mmc_cfg {
    u8 boot0_para;
    u8 boot_odly_50M;
    u8 boot_sdly_50M;
    u8 boot_odly_50M_ddr;
    u8 boot_sdly_50M_ddr;
    u8 boot_hs_f_max;
    u8 res[2];
};

struct boot_sdmmc_private_info_t {
    struct tune_sdly tune_sdly;
    struct boot_mmc_cfg boot_mmc_cfg;
    u32 card_type;
    u32 ext_para0;
    u32 ext_para1;
    u32 ext_para2;
    u32 ext_para3;
};

struct sunxi_sdmmc_parameter_region_header {
    u8 name[16];
    u32 version;
    u32 magic;
    u32 add_sum;
    u32 length;
    u8 reserved[16];
};

#define SUNXI_SDMMC_PARAMETER_REGION_LBA_START 24504
#define SUNXI_SDMMC_PARAMETER_REGION_SIZE_BYTE 512
struct sunxi_sdmmc_parameter_region {
    struct sunxi_sdmmc_parameter_region_header header;
    struct boot_sdmmc_private_info_t info;
};

#define mmc_host_is_spi(mmc) ((mmc)->host_caps & MMC_MODE_SPI)

struct sunxi_mmc {
    volatile u32 gctrl;         /* (0x00) SMC Global Control Register */
    volatile u32 clkcr;         /* (0x04) SMC Clock Control Register */
    volatile u32 timeout;       /* (0x08) SMC Time Out Register */
    volatile u32 width;         /* (0x0C) SMC Bus Width Register */
    volatile u32 blksz;         /* (0x10) SMC Block Size Register */
    volatile u32 bytecnt;       /* (0x14) SMC Byte Count Register */
    volatile u32 cmd;           /* (0x18) SMC Command Register */
    volatile u32 arg;           /* (0x1C) SMC Argument Register */
    volatile u32 resp0;         /* (0x20) SMC Response Register 0 */
    volatile u32 resp1;         /* (0x24) SMC Response Register 1 */
    volatile u32 resp2;         /* (0x28) SMC Response Register 2 */
    volatile u32 resp3;         /* (0x2C) SMC Response Register 3 */
    volatile u32 imask;         /* (0x30) SMC Interrupt Mask Register */
    volatile u32 mint;          /* (0x34) SMC Masked Interrupt Status Register */
    volatile u32 rint;          /* (0x38) SMC Raw Interrupt Status Register */
    volatile u32 status;        /* (0x3C) SMC Status Register */
    volatile u32 ftrglevel;     /* (0x40) SMC FIFO Threshold Watermark Register */
    volatile u32 funcsel;       /* (0x44) SMC Function Select Register */
    volatile u32 cbcr;          /* (0x48) SMC CIU Byte Count Register */
    volatile u32 bbcr;          /* (0x4C) SMC BIU Byte Count Register */
    volatile u32 dbgc;          /* (0x50) SMC Debug Enable Register */
    volatile u32 csdc;          /* (0x54) CRC status detect control register*/
    volatile u32 a12a;          /* (0x58) Auto command 12 argument*/
    volatile u32 ntsr;          /* (0x5c) SMC2 Newtiming Set Register */
    volatile u32 res1[6];       /* (0x54~0x74) */
    volatile u32 hwrst;         /* (0x78) SMC eMMC Hardware Reset Register */
    volatile u32 res2;          /* (0x7c) */
    volatile u32 dmac;          /* (0x80) SMC IDMAC Control Register */
    volatile u32 dlba;          /* (0x84) SMC IDMAC Descriptor List Base Address Register */
    volatile u32 idst;          /* (0x88) SMC IDMAC Status Register */
    volatile u32 idie;          /* (0x8C) SMC IDMAC Interrupt Enable Register */
    volatile u32 chda;          /* (0x90) */
    volatile u32 cbda;          /* (0x94) */
    volatile u32 res3[26];      /* (0x98~0xff) */
    volatile u32 thldc;         /* (0x100) Card Threshold Control Register */
    volatile u32 sfc;           /* (0x104) SMC Sample FIFO Control Register */
    volatile u32 res4[1];       /* (0x10b) */
    volatile u32 dsbd;          /* (0x10c) eMMC4.5 DDR Start Bit Detection Control */
    volatile u32 res5[12];      /* (0x110~0x13c) */
    volatile u32 drv_dl;        /* (0x140) drive delay control register*/
    volatile u32 samp_dl;       /* (0x144) sample delay control register*/
    volatile u32 ds_dl;         /* (0x148) data strobe delay control register */
    volatile u32 res6[45];      /* (0x110~0x1ff) */
    volatile u32 fifo;          /* (0x200) SMC FIFO Access Address */
};

struct sunxi_mmc_des {
    u32:1, dic:1,
    last_des:1,
    first_des:1,
    des_chain:1,
    end_of_ring:1,
    : 24,
    card_err_sum:1,
    own:1;
    u32 data_buf1_sz:16, data_buf2_sz:16;
    u32 buf_addr_ptr1;
    u32 buf_addr_ptr2;
};

struct sunxi_mmc_host {
    struct sunxi_mmc *reg;
    u32 mmc_no;
    u32 hclkrst;
    u32 hclkbase;
    u32 mclkbase;
    u32 database;
    u32 commreg;
    u32 fatal_err;
    struct sunxi_mmc_des *pdes;
    u32 timing_mode;
    struct mmc *mmc;
    u32 mod_clk;
    u32 clock;

};

struct tuning_sdly {
    u8 sdly_25M;
    u8 sdly_50M;
    u8 sdly_100M;
    u8 sdly_200M;
};

void mmc_update_host_caps_r(int sdc_no);

#endif
