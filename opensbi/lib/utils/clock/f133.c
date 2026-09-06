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
#include <sbi_utils/common/f133.h>
#include <sbi_utils/clock/f133.h>
#include <sbi_utils/serial/f133_uart.h>
#include <sbi_utils/display/f133_slcd.h>

static void set_pll_cpux_axi(void)
{
    unsigned int reg_val;
    void volatile *cpux_base = CCMU_CPUX_AXI_CFG_REG;
    void volatile *pll_base = CCMU_PLL_CPUX_CTRL_REG;

    // select CPUX  clock src: OSC24M,AXI divide ratio is 3, system apb clk ratio is 4
    writel((0 << 24) | (3 << 8) | (1 << 0), cpux_base);
    udelay(1);

    // disable pll gating
    reg_val = readl(pll_base);
    reg_val &= ~(1 << 27);
    writel(reg_val, pll_base);

    reg_val = readl(pll_base);
    reg_val |= (0x1U << 30);
    writel(reg_val, pll_base);
    udelay(5);

    // set default val: clk is 1008M, PLL_OUTPUT= 24M*N/( M*P)
    reg_val = readl(pll_base);
    reg_val &= ~((0x3 << 16) | (0xff << 8) | (0x3 << 0));
    reg_val |= (41 << 8);
    writel(reg_val, pll_base);

    // lock enable
    reg_val = readl(pll_base);
    reg_val |= (1 << 29);
    writel(reg_val, pll_base);

    // enable pll
    reg_val = readl(pll_base);
    reg_val |= (1 << 31);
    writel(reg_val, pll_base);

    // wait PLL_CPUX stable
    while (!(readl(pll_base) & (0x1 << 28))) {
    }
    udelay(20);
    
    // enable pll gating
    reg_val = readl(pll_base);
    reg_val |= (1 << 27);
    writel(reg_val, pll_base);

    // lock disable
    reg_val = readl(pll_base);
    reg_val &= ~(1 << 29);
    writel(reg_val, pll_base);

    udelay(1);
    
    // set and change cpu clk src to PLL_CPUX, PLL_CPUX:AXI0 = 1008M:504M
    reg_val = readl(cpux_base);
    reg_val &= ~(0x07 << 24 | 0x3 << 8 | 0xf << 0);
    reg_val |= (0x05 << 24 | 0x1 << 8);
    writel(reg_val, cpux_base);
    udelay(1);
}

static void set_pll_periph0(void)
{
    unsigned int reg_val;
    void volatile *pll_base = CCMU_PLL_PERI0_CTRL_REG;

    if ((1 << 31) & readl(pll_base)) {
        // fel has enable pll_periph0
        return;
    }
    
    // set default val
    writel(0x63 << 8, pll_base);

    // lock enable
    reg_val = readl(pll_base);
    reg_val |= (1 << 29);
    writel(reg_val, pll_base);

    // enabe PLL: 600M(1X), 1200M(2x)
    reg_val = readl(pll_base);
    reg_val |= (1 << 31);
    writel(reg_val, pll_base);

    while (!(readl(pll_base) & (0x1 << 28))) {
    }
    udelay(20);
    
    // lock disable
    reg_val = readl(pll_base);
    reg_val &= (~(1 << 29));
    writel(reg_val, pll_base);
}

static void set_ahb(void)
{
    void volatile *bus_base = CCMU_PSI_AHB1_AHB2_CFG_REG;

    // PLL6:AHB1:AHB2 = 600M:200M:200M
    writel((2 << 0) | (0 << 8), bus_base);
    writel((0x03 << 24) | readl(bus_base), bus_base);
    udelay(1);
}

static void set_apb(void)
{
    void volatile *bus_base = CCMU_APB1_CFG_GREG;

    // PLL6:APB1 = 600M:100M
    writel((2 << 0) | (1 << 8), bus_base);
    writel((0x03 << 24) | readl(bus_base), bus_base);
    udelay(1);
}

static void set_pll_dma(void)
{
    void volatile *dma_base = CCMU_DMA_BGR_REG;

    // dma reset
    writel(readl(dma_base) | (1 << 16), dma_base);
    udelay(20);
    
    // gating clock for dma pass
    writel(readl(dma_base) | (1 << 0), dma_base);
}

static void set_pll_mbus(void)
{
    unsigned int reg_val;
    void volatile *mbus_base = CCMU_MBUS_CFG_REG;

    // reset mbus domain
    reg_val = readl(mbus_base);
    reg_val |= (0x1 << 30);
    writel(reg_val, mbus_base);
    udelay(1);
}

void set_ldoa_analog(void)
{
    unsigned int ldoa_val = (readl(SUNXI_SID_BASE + 0x218) & 0xff);
    unsigned int sys_ldo_ctl = (readl(SUNXI_SYSCRL_BASE + 0x150) & 0xffffff00);

    if (ldoa_val) {
        writel(ldoa_val | sys_ldo_ctl, SUNXI_SYSCRL_BASE + 0x150);
    }
}

static void set_ldo_analog(void)
{
    unsigned int soc_version = (readl(SUNXI_SID_BASE + 0x200) >> 22) & 0x3f;
    unsigned int audio_codec_bg_trim = (readl(SUNXI_SID_BASE + 0x228) >> 16) & 0xff;

    clrbits(SUNXI_CCM_BASE + 0xA5C, 1 << (SUNXI_GATING_BIT));
    udelay(2);
    clrbits(SUNXI_CCM_BASE + 0xA5C, 1 << (SUNXI_RST_BIT));
    udelay(2);

    // deassert audio codec reset
    setbits(SUNXI_CCM_BASE + 0xA5C, 1 << (SUNXI_RST_BIT));
    
    // open the clock for audio codec
    setbits(SUNXI_CCM_BASE + 0xA5C, 1 << (SUNXI_GATING_BIT));

    if (soc_version == 0b1010 || soc_version == 0) {
        setbits(SUNXI_AUDIO_CODEC + 0x31C, 1 << 1);
        setbits(SUNXI_AUDIO_CODEC + 0x348, 1 << 30);
    }
    else {
        clrbits(SUNXI_AUDIO_CODEC + 0x31C, 3 << 0);
        clrbits(SUNXI_AUDIO_CODEC + 0x348, 1 << 30);
    }

    if (!audio_codec_bg_trim) {
        clrsetbits(SUNXI_AUDIO_CODEC + 0x348, 0xff, 0x19 << 0);
    }
    else {
        clrsetbits(SUNXI_AUDIO_CODEC + 0x348, 0xff, audio_codec_bg_trim << 0);
    }

}

static void set_circuits_analog(void)
{
}

static void set_vccio_detect(void)
{
    void volatile *vccio_det = SUNXI_RTC_BASE + 0x1f4;
    unsigned int value = 0x0;

    value = readl(vccio_det);

    if (!(value & 0x1)) {
        return;
    }

    // threshold value is 2.9v
    value &= ~(0x7 << 4);
    value |= (0x4 << 4);
    writel(value, vccio_det);

    // enable vccio detect
    value |= (0x1 << 7);
    writel(value, vccio_det);

    // not bypass
    value &= ~(1 << 0);
    writel(value, vccio_det);
}

static void set_platform_config(void)
{
    set_ldoa_analog();
    set_ldo_analog();
    set_circuits_analog();
    set_vccio_detect();
}

static void set_modules_clock(void)
{
    unsigned int reg_val, i;
    unsigned long ccmu_pll_addr[] = {
        CCMU_PLL_PERI0_CTRL_REG,
        CCMU_PLL_VIDE00_CTRL_REG,
        CCMU_PLL_VIDE01_CTRL_REG,
        CCMU_PLL_VE_CTRL_REG,
        CCMU_PLL_AUDIO0_CTRL_REG,
        CCMU_PLL_AUDIO1_CTRL_REG,
    };

    for (i = 0; i < sizeof(ccmu_pll_addr) / sizeof(ccmu_pll_addr[0]); i++) {
        reg_val = readl((const volatile void volatile *)ccmu_pll_addr[i]);
        if (!(reg_val & (1 << 31))) {
            writel(reg_val, (volatile void volatile *)ccmu_pll_addr[i]);

            reg_val = readl((const volatile void volatile *)ccmu_pll_addr[i]);
            writel(reg_val | (1 << 31) | (1 << 30), (volatile void volatile *)ccmu_pll_addr[i]);

            // lock enable
            reg_val = readl((const volatile void volatile *)ccmu_pll_addr[i]);
            reg_val |= (1 << 29);
            writel(reg_val, (volatile void volatile *)ccmu_pll_addr[i]);

            while (!(readl((const volatile void volatile *)ccmu_pll_addr[i]) & (0x1 << 28))) {
            }
            udelay(20);

            reg_val = readl((const volatile void volatile *)ccmu_pll_addr[i]);
            reg_val &= ~(1 << 29);
            writel(reg_val, (volatile void volatile *)ccmu_pll_addr[i]);
        }
    }
}

void f133_clock_init(void)
{
    set_platform_config();
    set_pll_cpux_axi();
    set_pll_periph0();
    set_ahb();
    set_apb();
    set_pll_dma();
    set_pll_mbus();
    set_modules_clock();
}

