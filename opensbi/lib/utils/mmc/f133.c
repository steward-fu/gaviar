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
#include <sbi_utils/mmc/mmc.h>
#include <sbi_utils/mmc/f133.h>
#include <sbi_utils/dram/f133.h>
#include <sbi_utils/gpio/f133.h>
#include <sbi_utils/clock/f133.h>
#include <sbi_utils/common/f133.h>
#include <sbi_utils/serial/f133_uart.h>
#include <sbi_utils/display/f133_slcd.h>

#define L1_CACHE_BYTES      64
#define SDXC_StartCal       (0x01 << 15)
#define SDXC_CalDone        (0x01 << 14)
#define SDXC_CalDly         (0x3f << 8)
#define SDXC_EnableDly      (0x01 << 7)
#define SDXC_CfgDly         (0x3f << 0)
#define SMC_DATA_TIMEOUT    0xffffffU
#define SMC_RESP_TIMEOUT    0xff

#define BOOT0_PARA_USE_INTERNAL_DEFAULT_TIMING_PARA (1 << 0)
#define BOOT0_PARA_USE_EXTERNAL_INPUT_TIMING_PARA   (1 << 1)

#define sunxi_r_op(mmchost, op) { \
    writel(readl(mmchost->mclkbase) & (~(1 << 31)), mmchost->mclkbase);\
    op;\
    writel(readl(mmchost->mclkbase) | (1 << 31), mmchost->mclkbase);\
}

extern char ext_csd[];
extern unsigned char mmc_arg_addr[];

int mmc_register(int dev_num, struct mmc *mmc);
int mmc_unregister(int dev_num);

struct mmc mmc_dev[MAX_MMC_NUM];
struct sunxi_mmc_host mmc_host[MAX_MMC_NUM];

static int mmc_clk_io_onoff(int sdc_no, int onoff, int offset)
{
    unsigned int rval = 0;
    struct sunxi_mmc_host *mmchost = &mmc_host[sdc_no];

    rval = readl(mmchost->hclkbase);
    rval|= (1 << (sdc_no));
    writel(rval, mmchost->hclkbase);

    rval = readl(mmchost->hclkrst);
    rval|= (1 << (16 + sdc_no));
    writel(rval, mmchost->hclkrst);

    writel(0x80000000, mmchost->mclkbase);
    mmchost->mod_clk = 24000000;
    return 0;
}

static int mmc_update_clk(struct mmc *mmc)
{
    unsigned int cmd;
    struct sunxi_mmc_host *mmchost = (struct sunxi_mmc_host *)mmc->priv;
    u32 timeout = timer_get_us() + 0xfffff;

    writel(readl(&mmchost->reg->clkcr) | (1 << 31), &mmchost->reg->clkcr);

    cmd = (1 << 31) | (1 << 21) | (1 << 13);
    writel(cmd, &mmchost->reg->cmd);
    while ((readl(&mmchost->reg->cmd) & 0x80000000) && (timer_get_us() < timeout)) {
    }

    if (readl(&mmchost->reg->cmd) & 0x80000000) {
        return -1;
    }

    writel(readl(&mmchost->reg->clkcr) & (~(0x1U << 31)), &mmchost->reg->clkcr);
    writel(readl(&mmchost->reg->rint), &mmchost->reg->rint);
    return 0;
}

static int mmc_update_phase(struct mmc *mmc)
{
    struct sunxi_mmc_host *mmchost = (struct sunxi_mmc_host *)mmc->priv;

    if (mmchost->timing_mode == SUNXI_MMC_TIMING_MODE_1) {
        return mmc_update_clk(mmc);
    }
    return 0;
}

static int mmc_get_timing_cfg_tm4(u32 sdc_no, u32 spd_md_id, u32 freq_id, u8 *odly, u8 *sdly)
{
    s32 ret = 0;
    u32 dly = 0;
    u8 boot0_para = 0;
    u32 spd_md_sdly = 0;

    struct boot_sdmmc_private_info_t *priv_info = &((struct sunxi_sdmmc_parameter_region *)mmc_arg_addr)->info;
    struct sunxi_sdmmc_parameter_region_header *region_header = &((struct sunxi_sdmmc_parameter_region *)mmc_arg_addr)->header;

    if (region_header->magic != SDMMC_PARAMETER_MAGIC) {
    } 
    else {
        boot0_para = priv_info->boot_mmc_cfg.boot0_para;
    }

    struct tune_sdly *tune_sdly = &(priv_info->tune_sdly);
    if (boot0_para & BOOT0_PARA_USE_INTERNAL_DEFAULT_TIMING_PARA) {
        if (spd_md_id == DS26_SDR12) {
            if (freq_id <= CLK_25M) {
                *odly = 0;
                *sdly = 2;
            } 
            else {
                ret = -1;
            }
        } 
        else if (spd_md_id == HSSDR52_SDR25) {
            if (freq_id <= CLK_25M) {
                *odly = 0;
                *sdly = 3;
            } 
            else if (freq_id == CLK_50M) {
                *odly = 0;
                *sdly = 4;
            } 
            else {
                ret = -1;
            }
        } 
        else {
            ret = -1;
        }
    } 
    else {
        if (boot0_para & BOOT0_PARA_USE_EXTERNAL_INPUT_TIMING_PARA) {
            if (spd_md_id == DS26_SDR12) {
                if (freq_id <= CLK_25M) {
                    *odly = 0;
                    *sdly = 2;
                } 
                else {
                    ret = -1;
                }
            } 
            else if (spd_md_id == HSSDR52_SDR25) {
                if (freq_id <= CLK_25M) {
                    *odly = 0;
                    *sdly = 3;
                } 
                else if (freq_id == CLK_50M) {
                    *odly = priv_info->boot_mmc_cfg
                            .boot_odly_50M;
                    *sdly = priv_info->boot_mmc_cfg
                            .boot_sdly_50M;
                } 
                else {
                    ret = -1;
                }
            }
            else if (spd_md_id == HSDDR52_DDR50) {
                if (freq_id <= CLK_25M) {
                    *odly = 0;
                    *sdly = 2;
                } 
                else if (freq_id == CLK_50M) {
                    *odly = priv_info->boot_mmc_cfg.boot_odly_50M_ddr;
                    *sdly = priv_info->boot_mmc_cfg.boot_sdly_50M_ddr;
                } 
                else {
                    ret = -1;
                }
            } 
            else {
                ret = -1;
            }
        } 
        else {
            if ((sdc_no != 2) || (spd_md_id > 4) || (freq_id > 5)) {
                ret = -1;
                goto out;
            }

            spd_md_sdly = tune_sdly->tm4_smx_fx[spd_md_id * 2 + freq_id / 4];
            dly = ((spd_md_sdly >> ((freq_id % 4) * 8)) & 0xff);

            if ((dly == 0xff) || (dly == 0)) {
                if (spd_md_id == DS26_SDR12) {
                    if (freq_id <= CLK_25M) {
                        dly = 0;
                    } 
                    else {
                        ret = -1;
                    }
                } 
                else if (spd_md_id == HSSDR52_SDR25) {
                    if (freq_id <= CLK_25M) {
                        dly = 0;
                    } 
                    else if (freq_id == CLK_50M) {
                        dly = 15;
                    } 
                    else {
                        ret = -1;
                    }
                } 
                else if (spd_md_id == HSDDR52_DDR50) {
                    if (freq_id <= CLK_25M) {
                        dly = 0;
                    } 
                    else {
                        ret = -1;
                    }
                } 
                else {
                    ret = -1;
                }
            }

            if (spd_md_id == HSDDR52_DDR50) {
                *odly = 1;
            }
            else {
                *odly = 0;
            }
            
            *sdly = dly;
        }
    }

out:
    return ret;
}

static int mmc_get_timing_cfg(u32 sdc_no, u32 spd_md_id, u32 freq_id, u8 *odly, u8 *sdly)
{
    s32 ret = 0;
    u32 tm  = mmc_host[sdc_no].timing_mode;

    if ((sdc_no == 2) && (tm == SUNXI_MMC_TIMING_MODE_4)) {
        return mmc_get_timing_cfg_tm4(sdc_no, spd_md_id, freq_id, odly, sdly);
    }
    else if ((sdc_no == 0) && (tm == SUNXI_MMC_TIMING_MODE_1)) {
        if ((spd_md_id <= HSSDR52_SDR25) && (freq_id <= CLK_50M)) {
            *odly = 0;
            *sdly = 0;
            ret   = 0;
        } 
        else {
            ret = -1;
        }
    } 
    else {
        ret = -1;
    }

    return ret;
}

static int _get_pll_periph0(void)
{
    unsigned int reg_val;
    int pll6, factor_n, factor_m, factor_p0;

    reg_val = readl(CCMU_PLL_PERI0_CTRL_REG);
    factor_n  = ((reg_val >> 8) & 0xff) + 1;
    factor_m  = ((reg_val >> 1) & 0x01) + 1;
    factor_p0 = ((reg_val >> 16) & 0x07) + 1;
    pll6 = 24 * factor_n / factor_m / factor_p0 / 2;
    return pll6;
}

static int mmc_set_mclk(struct sunxi_mmc_host *mmchost, u32 clk_hz)
{
    unsigned rval;
    unsigned n, m, div, src, sclk_hz = 0;


    if (clk_hz <= 4000000) {
        src     = 0;
        sclk_hz = 24000000;
    } 
    else {
        src     = 2;
        sclk_hz = _get_pll_periph0() * 2 * 1000000;
    }

    div = (2 * sclk_hz + clk_hz) / (2 * clk_hz);
    div = (div == 0) ? 1 : div;
    if (div > 128) {
        m = 1;
        n = 0;
    } 
    else if (div > 64) {
        n = 3;
        m = div >> 3;
    } 
    else if (div > 32) {
        n = 2;
        m = div >> 2;
    } 
    else if (div > 16) {
        n = 1;
        m = div >> 1;
    } 
    else {
        n = 0;
        m = div;
    }

    rval = (src << 24) | (n << 8) | (m - 1);
    writel(rval, mmchost->mclkbase);
    return 0;
}

static unsigned mmc_get_mclk(struct sunxi_mmc_host *mmchost)
{
    unsigned n, m, src, sclk_hz = 0;
    unsigned rval = readl(mmchost->mclkbase);

    m = rval & 0xf;
    n = (rval >> 8) & 0x3;
    src = (rval >> 24) & 0x3;

    if (src == 0) {
        sclk_hz = 24000000;
    }
    else if (src == 2) {
        sclk_hz = _get_pll_periph0() * 2 * 1000000;
    }
    return sclk_hz / (1 << n) / (m + 1);
}

static unsigned mmc_config_delay(struct sunxi_mmc_host *mmchost, u32 spd_md_id, u32 freq_id)
{
    int ret = 0;
    unsigned rval = 0;
    unsigned mode = mmchost->timing_mode;
    unsigned spd_md, spd_md_bak, freq;
    u8 odly, sdly, dsdly = 0;

    if (mode == SUNXI_MMC_TIMING_MODE_1) {
        odly = 0;
        sdly = 0;

        rval = readl(&mmchost->reg->drv_dl);
        rval &= (~(0x3 << 16));
        rval |= (((odly & 0x1) << 16) | ((odly & 0x1) << 17));
        sunxi_r_op(mmchost, writel(rval, &mmchost->reg->drv_dl));

        rval = readl(&mmchost->reg->ntsr);
        rval &= (~(0x3 << 4));
        rval |= ((sdly & 0x3) << 4);
        writel(rval, &mmchost->reg->ntsr);
    } 
    else if (mode == SUNXI_MMC_TIMING_MODE_3) {
        odly = 0;
        sdly = 0;

        rval = readl(&mmchost->reg->drv_dl);
        rval &= (~(0x3 << 16));
        rval |= (((odly & 0x1) << 16) | ((odly & 0x1) << 17));
        sunxi_r_op(mmchost, writel(rval, &mmchost->reg->drv_dl));

        rval = readl(&mmchost->reg->samp_dl);
        rval &= (~SDXC_CfgDly);
        rval |= ((sdly & SDXC_CfgDly) | SDXC_EnableDly);
        writel(rval, &mmchost->reg->samp_dl);
    } 
    else if (mode == SUNXI_MMC_TIMING_MODE_4) {
        spd_md     = spd_md_id;
        spd_md_bak = spd_md_id;
        freq       = freq_id;

        if (spd_md == HS400) {
            spd_md = HS200_SDR104;
        }

        odly = 0xff;
        sdly = 0xff;
        if (mmc_get_timing_cfg(mmchost->mmc_no, spd_md, freq, &odly, &sdly)) {
            ret = -1;
            goto OUT;
        }
        if ((odly == 0xff) || (sdly == 0xff)) {
            ret = -1;
            goto OUT;
        }

        rval = readl(&mmchost->reg->drv_dl);
        rval &= (~(0x3<<16));
        rval |= (((odly&0x1)<<16) | ((odly&0x1)<<17));
        sunxi_r_op(mmchost, writel(rval, &mmchost->reg->drv_dl));

        rval = readl(&mmchost->reg->samp_dl);
        rval &= (~SDXC_CfgDly);
        rval |= ((sdly & SDXC_CfgDly) | SDXC_EnableDly);
        writel(rval, &mmchost->reg->samp_dl);

        spd_md = spd_md_bak;
        if (spd_md == HS400) {
            odly  = 0xff;
            dsdly = 0xff;
            if (mmc_get_timing_cfg(mmchost->mmc_no, spd_md, freq, &odly, &dsdly)) {
                ret = -1;
                goto OUT;
            }
            if ((odly == 0xff) || (dsdly == 0xff)) {
                ret = -1;
                goto OUT;
            }

            rval = readl(&mmchost->reg->ds_dl);
            rval &= (~SDXC_CfgDly);
            rval |= ((dsdly & SDXC_CfgDly) | SDXC_EnableDly);
            writel(rval, &mmchost->reg->ds_dl);
        }
    }

OUT:
    return ret;
}

static int mmc_config_clock_modex(struct sunxi_mmc_host *mmchost, unsigned clk)
{
    unsigned freq;
    unsigned rval = 0;
    struct mmc *mmc = mmchost->mmc;
    unsigned mode = mmchost->timing_mode;

    writel(0x0, mmchost->mclkbase);

    if (mode == SUNXI_MMC_TIMING_MODE_1) {
        rval = readl(&mmchost->reg->ntsr);
        rval |= (1U << 31);
        writel(rval, &mmchost->reg->ntsr);
    } 
    else {
        writel(0x0, &mmchost->reg->ntsr);
    }

    if ((mode == SUNXI_MMC_TIMING_MODE_1) || (mode == SUNXI_MMC_TIMING_MODE_3)) {
        if (mmc->speed_mode == HSDDR52_DDR50) {
            mmchost->mod_clk = clk * 4;
        }
        else {
            mmchost->mod_clk = clk * 2;
        }
    } 
    else if (mode == SUNXI_MMC_TIMING_MODE_4) {
        if ((mmc->speed_mode == HSDDR52_DDR50) && (mmc->bus_width == 8)) {
            mmchost->mod_clk = clk * 4;
        }
        else {
            mmchost->mod_clk = clk * 2;
        }
    }
    mmc_set_mclk(mmchost, mmchost->mod_clk);

    if ((mode == SUNXI_MMC_TIMING_MODE_1) || (mode == SUNXI_MMC_TIMING_MODE_3)) {
        if (mmc->speed_mode == HSDDR52_DDR50) {
            mmc->clock = mmc_get_mclk(mmchost) / 4;
        }
        else {
            mmc->clock = mmc_get_mclk(mmchost) / 2;
        }
    } 
    else if (mode == SUNXI_MMC_TIMING_MODE_4) {
        if ((mmc->speed_mode == HSDDR52_DDR50) && (mmc->bus_width == 8)) {
            mmc->clock = mmc_get_mclk(mmchost) / 4;
        }
        else {
            mmc->clock = mmc_get_mclk(mmchost) / 2;
        }
    }
    mmchost->clock = mmc->clock;
    writel(readl(mmchost->mclkbase) | (1 << 31), mmchost->mclkbase);

    rval = readl(&mmchost->reg->clkcr);
    rval &= ~(0xFF);
    if ((mode == SUNXI_MMC_TIMING_MODE_1) || (mode == SUNXI_MMC_TIMING_MODE_3)) {
        if (mmc->speed_mode == HSDDR52_DDR50) {
            rval |= 0x1;
        }
    } 
    else if (mode == SUNXI_MMC_TIMING_MODE_4) {
        if ((mmc->speed_mode == HSDDR52_DDR50) && (mmc->bus_width == 8)) {
            rval |= 0x1;
        }
    }
    writel(rval, &mmchost->reg->clkcr);
    if (mmc_update_clk(mmc)) {
        return -1;
    }

    if (clk <= 400000) {
        freq = CLK_400K;
    }
    else if (clk <= 26000000) {
        freq = CLK_25M;
    }
    else if (clk <= 52000000) {
        freq = CLK_50M;
    }
    else if (clk <= 100000000) {
        freq = CLK_100M;
    }
    else if (clk <= 150000000) {
        freq = CLK_150M;
    }
    else if (clk <= 200000000) {
        freq = CLK_200M;
    }
    else {
        freq = CLK_25M;
    }
    mmc_config_delay(mmchost, mmc->speed_mode, freq);
    return 0;
}

static int mmc_config_clock(struct mmc *mmc, unsigned clk)
{
    unsigned rval = 0;
    struct sunxi_mmc_host *mmchost = (struct sunxi_mmc_host *)mmc->priv;

    if ((mmc->speed_mode == HSDDR52_DDR50) || (mmc->speed_mode == HS400)) {
        if (clk > mmc->f_max_ddr) {
            clk = mmc->f_max_ddr;
        }
    }

    rval = readl(&mmchost->reg->clkcr);
    rval &= ~(1 << 16);
    writel(rval, &mmchost->reg->clkcr);
    if (mmc_update_clk(mmchost->mmc)) {
        return -1;
    }

    if ((mmchost->timing_mode == SUNXI_MMC_TIMING_MODE_1) || 
        (mmchost->timing_mode == SUNXI_MMC_TIMING_MODE_3) ||
        (mmchost->timing_mode == SUNXI_MMC_TIMING_MODE_4)) 
    {
        mmc_config_clock_modex(mmchost, clk);
    }
    else {
        return -1;
    }

    rval = readl(&mmchost->reg->clkcr);
    rval |= (0x1 << 16);
    writel(rval, &mmchost->reg->clkcr);
    if (mmc_update_clk(mmchost->mmc)) {
        return -1;
    }
    return 0;
}

static void mmc_ddr_mode_onoff(struct mmc *mmc, int on)
{
    u32 rval = 0;
    struct sunxi_mmc_host *mmchost = (struct sunxi_mmc_host *)mmc->priv;

    rval = readl(&mmchost->reg->gctrl);
    rval &= (~(1 << 10));
    writel(readl(mmchost->mclkbase) & (~(1 << 31)), mmchost->mclkbase);

    if (on) {
        rval |= (1 << 10);
        writel(rval, &mmchost->reg->gctrl);
    } 
    else {
        writel(rval, &mmchost->reg->gctrl);
    }
    writel(readl(mmchost->mclkbase) | (1U << 31), mmchost->mclkbase);
}

static void mmc_hs400_mode_onoff(struct mmc *mmc, int on)
{
    u32 rval = 0;
    struct sunxi_mmc_host *mmchost = (struct sunxi_mmc_host *)mmc->priv;

    if (mmchost->mmc_no != 2) {
        return;
    }

    rval = readl(&mmchost->reg->dsbd);
    rval &= (~(1U << 31));

    if (on) {
        rval |= (1U << 31);
        writel(rval, &mmchost->reg->dsbd);
        rval = readl(&mmchost->reg->csdc);
        rval &= ~0xF;
        rval |= 0x6;
        writel(rval, &mmchost->reg->csdc);
    } 
    else {
        writel(rval, &mmchost->reg->dsbd);
        rval = readl(&mmchost->reg->csdc);
        rval &= ~0xF;
        rval |= 0x3;
        writel(rval, &mmchost->reg->csdc);
    }
}

static void mmc_set_ios(struct mmc *mmc)
{
    struct sunxi_mmc_host *mmchost = (struct sunxi_mmc_host *)mmc->priv;

    if (mmc->clock && mmc_config_clock(mmc, mmc->clock)) {
        mmchost->fatal_err = 1;
        return;
    }
    
    if (mmc->bus_width == 8) {
        writel(2, &mmchost->reg->width);
    }
    else if (mmc->bus_width == 4) {
        writel(1, &mmchost->reg->width);
    }
    else {
        writel(0, &mmchost->reg->width);
    }

    if (mmc->speed_mode == HSDDR52_DDR50) {
        mmc_ddr_mode_onoff(mmc, 1);
        mmc_hs400_mode_onoff(mmc, 0);
    } 
    else if (mmc->speed_mode == HS400) {
        mmc_ddr_mode_onoff(mmc, 0);
        mmc_hs400_mode_onoff(mmc, 1);
    } 
    else {
        mmc_ddr_mode_onoff(mmc, 0);
        mmc_hs400_mode_onoff(mmc, 0);
    }
}

static int mmc_core_init(struct mmc *mmc)
{
    u32 rval = 0;
    struct sunxi_mmc_host *mmchost = (struct sunxi_mmc_host *)mmc->priv;
    u32 timeout = timer_get_us() + 0xffff;

    writel(0x7, &mmchost->reg->gctrl);
    while (readl(&mmchost->reg->gctrl) & 0x7) {
        if (timer_get_us() > timeout) {
            return -1;
        }
    }

    writel((SMC_DATA_TIMEOUT << 8) | SMC_RESP_TIMEOUT, &mmchost->reg->timeout);
    writel((512 << 16) | (1 << 2) | (1 << 0), &mmchost->reg->thldc);
    writel(3, &mmchost->reg->csdc);
    writel(0xdeb, &mmchost->reg->dbgc);

    writel(1, &mmchost->reg->hwrst);
    writel(0, &mmchost->reg->hwrst);
    mdelay(1);
    writel(1, &mmchost->reg->hwrst);
    mdelay(1);

    if (mmc->control_num == 0) {
        rval = readl(&mmchost->reg->ntsr);
        rval |= (1U << 31);
        writel(rval, &mmchost->reg->ntsr);
        rval = readl(&mmchost->reg->drv_dl);
        sunxi_r_op(mmchost, writel(rval, &mmchost->reg->drv_dl));
    } 
    else {
        rval = readl(&mmchost->reg->samp_dl);
        rval &= ~(0x3F);
        rval |= (1U << 7);
        writel(rval, &mmchost->reg->samp_dl);
        rval = readl(&mmchost->reg->drv_dl);
        sunxi_r_op(mmc_host, writel(rval, &mmchost->reg->drv_dl));
    }
    return 0;
}

static int mmc_trans_data_by_cpu(struct mmc *mmc, struct mmc_data *data)
{
    unsigned i;
    unsigned *buff;
    struct sunxi_mmc_host *mmchost = (struct sunxi_mmc_host *)mmc->priv;
    unsigned byte_cnt = data->blocksize * data->blocks;
    unsigned timeout = timer_get_us() +  0xffffff;

    if (data->flags & MMC_DATA_READ) {
        buff = (unsigned int *)data->b.dest;
        for (i = 0; i < (byte_cnt >> 2); i++) {
            while ((readl(&mmchost->reg->status) & (1 << 2)) && (timer_get_us() < timeout)) {
            }
            if (readl(&mmchost->reg->status) & (1 << 2)) {
                goto out;
            }
            buff[i] = readl(mmchost->database);
            timeout = timer_get_us() + 0xffffff;
        }
    } 
    else {
        buff = (unsigned int *)data->b.src;
        for (i = 0; i < (byte_cnt >> 2); i++) {
            while ((readl(&mmchost->reg->status) & (1 << 3)) && (timer_get_us() < timeout)) {
            }
            if (readl(&mmchost->reg->status) & (1 << 3)) {
                goto out;
            }
            writel(buff[i], mmchost->database);
            timeout = timer_get_us() +  0xffffff;
        }
    }

out:
    if (timer_get_us() >= timeout) {
        return -1;
    }
    return 0;
}

static int mmc_trans_data_by_dma(struct mmc *mmc, struct mmc_data *data)
{
    struct sunxi_mmc_host *mmchost = (struct sunxi_mmc_host *)mmc->priv;
    struct sunxi_mmc_des *pdes = NULL;
    unsigned byte_cnt = data->blocksize * data->blocks;
    unsigned char *buff;
    unsigned des_idx = 0;
    unsigned buff_frag_num = 0;
    unsigned remain;
    unsigned i, rval;
    u32 timeout = 0;

    buff = data->flags & MMC_DATA_READ ? (unsigned char *)data->b.dest : (unsigned char *)data->b.src;
    buff_frag_num = byte_cnt >> SDXC_DES_NUM_SHIFT;
    remain = byte_cnt & (SDXC_DES_BUFFER_MAX_LEN - 1);
    if (remain) {
        buff_frag_num++;
    }
    else {
        remain = SDXC_DES_BUFFER_MAX_LEN;
    }
    pdes = mmchost->pdes;

    if (data->flags & MMC_DATA_WRITE) {
        //OSAL_CacheRangeFlush(buff, (unsigned long)byte_cnt, 0);
    }
    for (i = 0; i < buff_frag_num; i++, des_idx++) {
        __memset((void *)&pdes[des_idx], 0, sizeof(struct sunxi_mmc_des));
        pdes[des_idx].des_chain = 1;
        pdes[des_idx].own = 1;
        pdes[des_idx].dic = 1;
        if (buff_frag_num > 1 && i != buff_frag_num - 1) {
            pdes[des_idx].data_buf1_sz = SDXC_DES_BUFFER_MAX_LEN;
        } 
        else {
            pdes[des_idx].data_buf1_sz = remain;
        }

        pdes[des_idx].buf_addr_ptr1 = ((size_t)buff + i * SDXC_DES_BUFFER_MAX_LEN) >> 2;
        if (i == 0) {
            pdes[des_idx].first_des = 1;
        }

        if (i == buff_frag_num - 1) {
            pdes[des_idx].dic       = 0;
            pdes[des_idx].last_des      = 1;
            pdes[des_idx].end_of_ring   = 1;
            pdes[des_idx].buf_addr_ptr2 = 0;
        } 
        else {
            pdes[des_idx].buf_addr_ptr2 =
                ((size_t)&pdes[des_idx + 1]) >> 2;
        }
    }
    WR_MB();
    rval = readl(&mmchost->reg->gctrl);
    writel(rval | (1 << 5) | (1 << 2), &mmchost->reg->gctrl);
    timeout = timer_get_us() + 0xffff;
    while (readl(&mmchost->reg->gctrl) & (1 << 2)) {
        if (timer_get_us() > timeout) {
            return -1;
        }
    }
    writel((1 << 0), &mmchost->reg->dmac);
    timeout = timer_get_us() + 0xffff;
    while (readl(&mmchost->reg->dmac) & (1 << 0)) {
        if (timer_get_us() > timeout) {
            return -1;
        }
    }
    writel((1 << 1) | (1 << 7), &mmchost->reg->dmac);
    rval = readl(&mmchost->reg->idie) & (~3);
    if (data->flags & MMC_DATA_WRITE) {
        rval |= (1 << 0);
    }
    else {
        rval |= (1 << 1);
    }
    writel(rval, &mmchost->reg->idie);
    writel(((size_t)pdes) >> 2, &mmchost->reg->dlba);
    writel((3U << 28) | (15 << 16) | 240, &mmchost->reg->ftrglevel);
    return 0;
}

static int mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
{
    int error = 0;
    struct sunxi_mmc_host *mmchost = (struct sunxi_mmc_host *)mmc->priv;
    unsigned int cmdval = 0x80000000;
    unsigned int timeout = 0;
    unsigned int status = 0;
    unsigned int usedma = 0;
    unsigned int bytecnt = 0;

    if (mmchost->fatal_err) {
        return -1;
    }
    
    if ((cmd->cmdidx == 12) && !(cmd->flags & MMC_CMD_MANUAL)) {
        return 0;
    }
    
    if (!cmd->cmdidx) {
        cmdval |= (1 << 15);
    }

    if (cmd->resp_type & MMC_RSP_PRESENT) {
        cmdval |= (1 << 6);
    }

    if (cmd->resp_type & MMC_RSP_136) {
        cmdval |= (1 << 7);
    }

    if (cmd->resp_type & MMC_RSP_CRC) {
        cmdval |= (1 << 8);
    }

    if (data) {
        if ((long)data->b.dest & 0x3) {
            error = -1;
            goto out;
        }

        cmdval |= (1 << 9) | (1 << 13);
        if (data->flags & MMC_DATA_WRITE) {
            cmdval |= (1 << 10);
        }
        if (data->blocks > 1) {
            cmdval |= (1 << 12);
        }
        writel(data->blocksize, &mmchost->reg->blksz);
        writel(data->blocks * data->blocksize, &mmchost->reg->bytecnt);
    } 
    else {
        if ((cmd->cmdidx == 12) && (cmd->flags & MMC_CMD_MANUAL)) {
            cmdval |= 1 << 14;
            cmdval &= ~(1 << 13);
        }
    }

    writel(cmd->cmdarg, &mmchost->reg->arg);
    if (!data) {
        writel(cmdval | cmd->cmdidx, &mmchost->reg->cmd);
    }

    if (data) {
        int ret = 0;
        bytecnt = data->blocksize * data->blocks;

        if (bytecnt > 512) {
            usedma = 1;
            writel(readl(&mmchost->reg->gctrl) & (~0x80000000), &mmchost->reg->gctrl);
            ret = mmc_trans_data_by_dma(mmc, data);
            writel(cmdval | cmd->cmdidx, &mmchost->reg->cmd);
        } 
        else {
            writel(readl(&mmchost->reg->gctrl) | 0x80000000, &mmchost->reg->gctrl);
            writel(cmdval | cmd->cmdidx, &mmchost->reg->cmd);
            ret = mmc_trans_data_by_cpu(mmc, data);
        }
        if (ret) {
            error = readl(&mmchost->reg->rint) & 0xbbc2;
            if (!error) {
                error = 0xffffffff;
            }
            goto out;
        }
    }

    timeout = timer_get_us() + 0xffffff;
    do {
        status = readl(&mmchost->reg->rint);
        if ((timer_get_us() > timeout) || (status & 0xbbc2)) {
            error = status & 0xbbc2;
            if (!error) {
                error = 0xffffffff;
            }
            goto out;
        }
    } while (!(status & 0x4));

    if (data) {
        unsigned done = 0;
        timeout =  timer_get_us() + (usedma ? 0xffffff : 0xffff);

        do {
            status = readl(&mmchost->reg->rint);
            if ((timer_get_us() > timeout) || (status & 0xbbc2)) {
                error = status & 0xbbc2;
                if (!error) {
                    error = 0xffffffff;
                }
                goto out;
            }
            if (data->blocks > 1) {
                done = status & (1 << 14);
            }
            else {
                done = status & (1 << 3);
            }
        } while (!done);

        if ((data->flags & MMC_DATA_READ) && usedma) {
            timeout = timer_get_us() + 0xffffff;
            done = 0;
            status  = 0;
            do {
                status = readl(&mmchost->reg->idst);
                if ((timer_get_us() > timeout) || (status & 0x234)) {
                    error = status & 0x1E34;
                    if (!error) {
                        error = 0xffffffff;
                    }
                    goto out;
                }
                done = status & (1 << 1);
            } while (!done);
        }
    }

    if (cmd->resp_type & MMC_RSP_BUSY) {
        timeout = timer_get_us() + 0x4ffffff;
        do {
            status = readl(&mmchost->reg->status);
            if (timer_get_us() > timeout) {
                error = -1;
                goto out;
            }
        } while (status & (1 << 9));
    }
    if (cmd->resp_type & MMC_RSP_136) {
        cmd->response[0] = readl(&mmchost->reg->resp3);
        cmd->response[1] = readl(&mmchost->reg->resp2);
        cmd->response[2] = readl(&mmchost->reg->resp1);
        cmd->response[3] = readl(&mmchost->reg->resp0);
    } 
    else {
        cmd->response[0] = readl(&mmchost->reg->resp0);
    }
out:
    if (data && usedma) {
        status = readl(&mmchost->reg->idst);
        writel(status, &mmchost->reg->idst);
        writel(0, &mmchost->reg->idie);
        writel(0, &mmchost->reg->dmac);
        writel(readl(&mmchost->reg->gctrl) & (~(1 << 5)), &mmchost->reg->gctrl);
        if (data->flags & MMC_DATA_READ) {
            // OSAL_CacheRangeInvaild(data->b.dest, data->blocksize * data->blocks, 0);
        }
    }
    if (error) {
        writel(0x7, &mmchost->reg->gctrl);
        timeout = timer_get_us() + 0xffff;
        while (readl(&mmchost->reg->gctrl) & 0x7) {
            if (timer_get_us() > timeout) {
                return -1;
            }
        }
        mmc_update_clk(mmc);
    }
    writel(0xffffffff, &mmchost->reg->rint);

    if (error) {
        return -1;
    }
    return 0;
}

void mmc_update_host_caps_r(int sdc_no)
{
    struct mmc *mmc = NULL;
    u8 sdly = 0xff, odly = 0xff;
    struct boot_sdmmc_private_info_t *priv_info = &((struct sunxi_sdmmc_parameter_region *)mmc_arg_addr)->info;
    u8 ext_f_max = priv_info->boot_mmc_cfg.boot_hs_f_max;
    
    mmc = &mmc_dev[sdc_no];
    if (!mmc_get_timing_cfg(sdc_no, 1, 2, &odly, &sdly)) {
        if (!((odly != 0xff) && (sdly != 0xff))) {
            mmc->f_max = 25000000;
        }
    } 
    else {
        mmc->f_max = 25000000;
    }

    if (!mmc_get_timing_cfg(sdc_no, 2, 2, &odly, &sdly)) {
        if ((odly != 0xff) && (sdly != 0xff)) {
        }
        else {
            mmc->f_max_ddr = 25000000;
        }
    } 
    else {
        mmc->f_max_ddr = 25000000;
    }

    if (ext_f_max && ((mmc->f_max_ddr / 1000000) > ext_f_max)) {
        mmc->f_max_ddr = ext_f_max * 1000000;
    }
    if (ext_f_max && ((mmc->f_max / 1000000) > ext_f_max)) {
        mmc->f_max = ext_f_max * 1000000;
    }
    if (priv_info->ext_para1 & EXT_PARA1_1V8_GPIO_BIAS) {
        writel(readl(SUNXI_PIO_BASE + GPIO_POW_MODE_REG) | (1 << 2), SUNXI_PIO_BASE + GPIO_POW_MODE_REG);
    }

    if (((priv_info->ext_para0 & 0xFF000000) != EXT_PARA0_ID) || !(priv_info->ext_para0 & EXT_PARA0_TUNING_SUCCESS_FLAG)) {
        mmc->f_max = 25000000;
        mmc->f_max_ddr = 25000000;
    }
}

int f133_mmc_init(void)
{
    struct mmc *mmc = NULL;
    const int sdc_no = 0;
    const int bus_width = 4;

    writel(0x00222222, SUNXI_PIO_BASE + 0xf0);
    writel(0x00222222, SUNXI_PIO_BASE + 0x104);
    writel(0x00000555, SUNXI_PIO_BASE + 0x114);

    __memset(&mmc_dev[sdc_no], 0, sizeof(struct mmc));
    __memset(&mmc_host[sdc_no], 0, sizeof(struct sunxi_mmc_host));
    mmc = &mmc_dev[sdc_no];
    mmc_host[sdc_no].mmc = mmc;
    mmc_host[sdc_no].timing_mode = SUNXI_MMC_TIMING_MODE_1;

    strcpy(mmc->name, "MMC");
    mmc->priv = &mmc_host[sdc_no];
    mmc->send_cmd = mmc_send_cmd;
    mmc->set_ios = mmc_set_ios;
    mmc->init = mmc_core_init;
    mmc->update_phase = mmc_update_phase;
    mmc->voltages = MMC_VDD_29_30 | MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_34_35 | MMC_VDD_35_36;
    mmc->host_caps = MMC_MODE_HS_52MHz | MMC_MODE_HS | MMC_MODE_HC;
    if (bus_width >= 4) {
        mmc->host_caps |= MMC_MODE_4BIT;
    }

    mmc->f_min = 400000;
    mmc->f_max = 50000000;
    mmc->f_max_ddr = 50000000;
    mmc->control_num = sdc_no;
    mmc_host[sdc_no].pdes = (struct sunxi_mmc_des *)DMAC_DES_BASE_IN_SDRAM;
    mmc_host[sdc_no].reg = (struct sunxi_mmc*)(long)(MMC_REG_BASE + sdc_no * 0x1000);
    mmc_host[sdc_no].database = (long)(mmc_host[sdc_no].reg) + MMC_REG_FIFO_OS;
    mmc_host[sdc_no].hclkbase = CCMU_HCLKGATE0_BASE;
    mmc_host[sdc_no].hclkrst  = CCMU_HCLKRST0_BASE;
    mmc_host[sdc_no].mclkbase = CCMU_MMC0_CLK_BASE;
    mmc_host[sdc_no].mmc_no = sdc_no;

    mmc_clk_io_onoff(sdc_no, 1, 16);
    if (mmc_register(sdc_no, mmc) < 0) {
        return -1;
    }
    printf("[eMMC] MicroSD size = %ldMB\n", mmc->lba / 2048);
    return mmc->lba;
}

void f133_mmc_reload(void)
{
    printf("[eMMC] Copy DTB to 0x%08x (size = %d)...\n", DTB_BASE, DTB_SIZE);
    mmc_bread(0, DTB_LBA, (DTB_SIZE / 512) + 1, DTB_BASE);

    printf("[eMMC] Copy Kernel to 0x%08x (size = %d)...\n", KERNEL_BASE, KERNEL_SIZE);
    mmc_bread(0, KERNEL_LBA, (KERNEL_SIZE / 512) + 1, KERNEL_BASE);

    printf("[eMMC] Copy OpenSBI to 0x%08x (size = %d)...\n", OPENSBI_BASE, OPENSBI_SIZE);
    mmc_bread(0, OPENSBI_LBA, (OPENSBI_SIZE / 512) + 1, OPENSBI_BASE);
}

void f133_mmc_exit(void)
{
    const int sdc_no = 0;
    const int offset = 16;

    mmc_clk_io_onoff(sdc_no, 0, offset);
    mmc_unregister(sdc_no);
    __memset(&mmc_dev[sdc_no], 0, sizeof(struct mmc));
    __memset(&mmc_host[sdc_no], 0, sizeof(struct sunxi_mmc_host));
}
