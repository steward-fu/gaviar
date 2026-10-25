/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option)any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/console.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/lcm.h>
#include <linux/clk-provider.h>
#include <video/of_display_timing.h>
#include <linux/gpio.h>
#include <linux/omapfb.h>
#include <linux/compiler.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/irq.h>

#define DRIVER_NAME         "gaviar-fb"
#define CCU_BASE            0x02001000
#define GPIO_BASE           0x02000000
#define TCON_BASE           0x05461000
#define LCD_GCTL_REG        0x0000
#define LCD_GINT0_REG       0x0004
#define LCD_GINT1_REG       0x0008
#define LCD_FRM_CTL_REG     0x0010
#define LCD_3D_FIFO_REG     0x003C
#define LCD_CTL_REG         0x0040
#define LCD_DCLK_REG        0x0044
#define LCD_BASIC0_REG      0x0048
#define LCD_BASIC1_REG      0x004C
#define LCD_BASIC2_REG      0x0050
#define LCD_BASIC3_REG      0x0054
#define LCD_HV_IF_REG       0x0058
#define LCD_CPU_IF_REG      0x0060
#define LCD_CPU_WR_REG      0x0064
#define LCD_IO_POL_REG      0x0088
#define LCD_IO_TRI_REG      0x008C
#define LCD_FRM_SEED_REG0   (0x0014 + 0 * 0x04)
#define LCD_FRM_SEED_REG1   (0x0014 + 1 * 0x04)
#define LCD_FRM_SEED_REG2   (0x0014 + 2 * 0x04)
#define LCD_FRM_SEED_REG3   (0x0014 + 3 * 0x04)
#define LCD_FRM_SEED_REG4   (0x0014 + 4 * 0x04)
#define LCD_FRM_SEED_REG5   (0x0014 + 5 * 0x04)
#define LCD_FRM_TAB_REG0    (0x002C + 0 * 0x04)
#define LCD_FRM_TAB_REG1    (0x002C + 1 * 0x04)
#define LCD_FRM_TAB_REG2    (0x002C + 2 * 0x04)
#define LCD_FRM_TAB_REG3    (0x002C + 3 * 0x04)
#define PLL_CPU_CTRL_REG    0x0000
#define PLL_PERI_CTRL_REG   0x0020
#define PLL_VIDEO0_CTRL_REG 0x0040
#define PLL_VIDEO1_CTRL_REG 0x0048
#define LCD_CEU_CTL_REG     0x0100
#define LCD_CPU_TRI0_REG    0x0160
#define LCD_CPU_TRI1_REG    0x0164
#define LCD_CPU_TRI2_REG    0x0168
#define LCD_CPU_TRI3_REG    0x016c
#define LCD_CPU_TRI4_REG    0x0170
#define LCD_CPU_TRI5_REG    0x0174
#define LCD_LVDS_IF_REG     0x0084
#define LCD_SYNC_CTL_REG    0x0230
#define LCD_SYNC_POS_REG    0x0234
#define DRAM_BGR_REG        0x080C
#define G2D_CLK_REG         0x0630
#define G2D_BGR_REG         0x063C
#define HSTIMER_BGR_REG     0x073C
#define TCONLCD_CLK_REG     0x0B60
#define TCONLCD_BGR_REG     0x0B7C
#define RISC_CLK_REG        0x0D00
#define RISC_GATING_REG     0x0D04
#define RISC_CFG_BGR_REG    0x0D0C
#define PD_CFG0             0x0090
#define PD_CFG1             0x0094
#define PD_CFG2             0x0098
#define PD_DAT              0x00a0
#define PD_PULL0            0x00B4
#define PD_PULL1            0x00B8
#define LCD_RST             (1 << 0)
#define LCD_WR              (1 << 18)
#define LCD_RS              (1 << 19)
#define LCD_RD              (1 << 20)
#define LCD_CS              (1 << 21)
#define LCD_BL              (1 << 22)

#define PALETTE_SIZE 256

struct myfb_par {
    struct device *dev;
    struct platform_device *pdev;

    resource_size_t p_palette_base;
    unsigned short *v_palette_base;

    int bpp;
    void *vram_virt;
    uint32_t vram_size;
    dma_addr_t vram_phys;
    uint32_t pseudo_palette[16];
    struct fb_videomode mode;
};

struct iomm {
    uint8_t *ccu;
    uint8_t *gpio;
    uint8_t *tcon;
};

static struct iomm myio = { 0 };
static struct myfb_par *mypar = NULL;
static struct fb_var_screeninfo myfb_var = { 0 };
static struct fb_fix_screeninfo myfb_fix = {
    .id = DRIVER_NAME,
    .type = FB_TYPE_PACKED_PIXELS,
    .type_aux = 0,
    .visual = FB_VISUAL_TRUECOLOR,
    .xpanstep = 0,
    .ypanstep = 1,
    .ywrapstep = 0,
    .accel = FB_ACCEL_NONE
};

void sun20i_clrbits(void __iomem *reg, u32 clr_val)
{
    uint32_t ret;

    ret = readl(reg);
    ret &= ~(clr_val);
    writel(ret, reg);
}

void sun20i_setbits(void __iomem *reg, u32 set_val)
{
    uint32_t ret;

    ret = readl(reg);
    ret |= set_val;
    writel(ret, reg);
}

void sun20i_clrsetbits(void __iomem *reg, u32 clr_val, u32 set_val)
{
    uint32_t ret;

    ret = readl(reg);
    ret &= ~(clr_val);
    ret |= set_val;
    writel(ret, reg);
}

static uint32_t tcon_wait_busy(void)
{
    uint32_t cnt = 0;

    sun20i_setbits(myio.tcon + LCD_CPU_IF_REG, (1 << 0));
    ndelay(10);

    while (1) {
        if (readl(myio.tcon + LCD_CPU_IF_REG) & 0x00c00000) {
            if (cnt > 200) {
                return -1;
            }
            else {
                cnt += 1;
            }
        }
        break;
    }
    return 0;
}

static uint32_t extend_16b_to_24b(uint32_t value)
{
    return ((value & 0xfc00) << 8) |
        ((value & 0x0300) << 6) |
        ((value & 0x00e0) << 5) |
        ((value & 0x001f) << 3);
}

static void tcon_write(uint8_t is_dat, uint32_t dat)
{
    while (tcon_wait_busy());

    if (is_dat) {
        sun20i_setbits(myio.tcon + LCD_CPU_IF_REG, (1 << 25));
    }
    else {
        sun20i_clrbits(myio.tcon + LCD_CPU_IF_REG, (1 << 25));
    }

    while (tcon_wait_busy());
    writel(extend_16b_to_24b(dat), myio.tcon + LCD_CPU_WR_REG);
}

static void gpio_write(uint32_t ctl, uint32_t dat)
{
    uint32_t r = ctl | ((dat & 0xff) << 1) | ((dat & 0xff00) << 2);

    writel(r, myio.gpio + PD_DAT);
    r |= LCD_WR;
    writel(r, myio.gpio + PD_DAT);
}

static void lcd_write_cmd(uint32_t cmd)
{
    gpio_write(LCD_RD | LCD_BL | LCD_RST, cmd);
}

static void lcd_write_dat(uint32_t dat)
{
    gpio_write(LCD_RS | LCD_RD | LCD_BL | LCD_RST, dat);
}

static int tcon_init(void)
{
    writel(0xf8000b00, myio.ccu + PLL_VIDEO0_CTRL_REG);
    writel(0xf8006202, myio.ccu + PLL_VIDEO1_CTRL_REG);
    writel(0x81000000, myio.ccu + TCONLCD_CLK_REG);
    writel(0x10001, myio.ccu + TCONLCD_BGR_REG);

    writel(0x80000000, myio.tcon + LCD_GCTL_REG);
    writel(0x0, myio.tcon + LCD_GINT0_REG);
    writel(0x0, myio.tcon + LCD_GINT1_REG);
    writel(0x80000050, myio.tcon + LCD_FRM_CTL_REG);
    writel(0x1, myio.tcon + LCD_FRM_SEED_REG0);
    writel(0x3, myio.tcon + LCD_FRM_SEED_REG1);
    writel(0x5, myio.tcon + LCD_FRM_SEED_REG2);
    writel(0x7, myio.tcon + LCD_FRM_SEED_REG3);
    writel(0xb, myio.tcon + LCD_FRM_SEED_REG4);
    writel(0xd, myio.tcon + LCD_FRM_SEED_REG5);
    writel(0x1010000, myio.tcon + LCD_FRM_TAB_REG0);
    writel(0x15151111, myio.tcon + LCD_FRM_TAB_REG1);
    writel(0x57575555, myio.tcon + LCD_FRM_TAB_REG2);
    writel(0x7f7f7777, myio.tcon + LCD_FRM_TAB_REG3);
    writel(0x0, myio.tcon + LCD_3D_FIFO_REG);
    writel(0x81000000, myio.tcon + LCD_CTL_REG);
    writel(0xf0000030, myio.tcon + LCD_DCLK_REG);
    writel(0x13f00ef, myio.tcon + LCD_BASIC0_REG);
    writel(0x0, myio.tcon + LCD_BASIC1_REG);
    writel(0x0, myio.tcon + LCD_BASIC2_REG);
    writel(0x0, myio.tcon + LCD_BASIC3_REG);
    writel(0x0, myio.tcon + LCD_HV_IF_REG);
    writel(0x86000005, myio.tcon + LCD_CPU_IF_REG);
    writel(0x0, myio.tcon + LCD_LVDS_IF_REG);
    writel(0x0, myio.tcon + LCD_IO_POL_REG);
    writel(0x0, myio.tcon + LCD_IO_TRI_REG);
    writel(0x0, myio.tcon + LCD_CEU_CTL_REG);
    writel(0x3f013f, myio.tcon + LCD_CPU_TRI0_REG);
    writel(0xef, myio.tcon + LCD_CPU_TRI1_REG);
    writel(0x6720013f, myio.tcon + LCD_CPU_TRI2_REG);
    writel(0x10617f00, myio.tcon + LCD_CPU_TRI3_REG);
    writel(0x0, myio.tcon + LCD_CPU_TRI4_REG);
    writel(0x0, myio.tcon + LCD_CPU_TRI5_REG);
    writel(0x0, myio.tcon + LCD_SYNC_CTL_REG);
    writel(0x0, myio.tcon + LCD_SYNC_POS_REG);

    printk("======\n");
    printk("PLL_CPU_CTRL_REG = 0x%x\n", readl(myio.ccu + 0x0000));
    printk("PLL_PERI_CTRL_REG = 0x%x\n", readl(myio.ccu + 0x0020));
    printk("PLL_VIDEO0_CTRL_REG = 0x%x\n", readl(myio.ccu + 0x0040));
    printk("PLL_VIDEO1_CTRL_REG = 0x%x\n", readl(myio.ccu + 0x0048));
    printk("G2D_CLK_REG = 0x%x\n", readl(myio.ccu + 0x0630));
    printk("G2D_BGR_REG = 0x%x\n", readl(myio.ccu + 0x063C));
    printk("TCONLCD_CLK_REG = 0x%x\n", readl(myio.ccu + 0x0B60));
    printk("TCONLCD_BGR_REG = 0x%x\n", readl(myio.ccu + 0x0B7C));

    printk("LCD_GCTL_REG = 0x%x\n", readl(myio.tcon + 0x000));
    printk("LCD_GINT0_REG = 0x%x\n", readl(myio.tcon + 0x004));
    printk("LCD_GINT1_REG = 0x%x\n", readl(myio.tcon + 0x008));
    printk("LCD_FRM_CTL_REG = 0x%x\n", readl(myio.tcon + 0x010));
    printk("LCD_FRM_SEED_REG-0 = 0x%x\n", readl(myio.tcon + 0x0014 + 0 * 0x04));
    printk("LCD_FRM_SEED_REG-1 = 0x%x\n", readl(myio.tcon + 0x0014 + 1 * 0x04));
    printk("LCD_FRM_SEED_REG-2 = 0x%x\n", readl(myio.tcon + 0x0014 + 2 * 0x04));
    printk("LCD_FRM_SEED_REG-3 = 0x%x\n", readl(myio.tcon + 0x0014 + 3 * 0x04));
    printk("LCD_FRM_SEED_REG-4 = 0x%x\n", readl(myio.tcon + 0x0014 + 4 * 0x04));
    printk("LCD_FRM_SEED_REG-5 = 0x%x\n", readl(myio.tcon + 0x0014 + 5 * 0x04));
    printk("LCD_FRM_TAB_REG-0 = 0x%x\n", readl(myio.tcon + 0x002C + 0 * 0x04));
    printk("LCD_FRM_TAB_REG-1 = 0x%x\n", readl(myio.tcon + 0x002C + 1 * 0x04));
    printk("LCD_FRM_TAB_REG-2 = 0x%x\n", readl(myio.tcon + 0x002C + 2 * 0x04));
    printk("LCD_FRM_TAB_REG-3 = 0x%x\n", readl(myio.tcon + 0x002C + 3 * 0x04));
    printk("LCD_3D_FIFO_REG = 0x%x\n", readl(myio.tcon + 0x003C));
    printk("LCD_CTL_REG = 0x%x\n", readl(myio.tcon + 0x0040));
    printk("LCD_DCLK_REG = 0x%x\n", readl(myio.tcon + 0x0044));
    printk("LCD_BASIC0_REG = 0x%x\n", readl(myio.tcon + 0x0048));
    printk("LCD_BASIC1_REG = 0x%x\n", readl(myio.tcon + 0x004c));
    printk("LCD_BASIC2_REG = 0x%x\n", readl(myio.tcon + 0x0050));
    printk("LCD_BASIC3_REG = 0x%x\n", readl(myio.tcon + 0x0054));
    printk("LCD_HV_IF_REG = 0x%x\n", readl(myio.tcon + 0x0058));
    printk("LCD_CPU_IF_REG = 0x%x\n", readl(myio.tcon + 0x0060));
    printk("LCD_CPU_WR_REG = 0x%x\n", readl(myio.tcon + 0x0064));
    printk("LCD_CPU_RD0_REG = 0x%x\n", readl(myio.tcon + 0x0068));
    printk("LCD_CPU_RD1_REG = 0x%x\n", readl(myio.tcon + 0x006c));
    printk("LCD_LVDS_IF_REG = 0x%x\n", readl(myio.tcon + 0x0084));
    printk("LCD_IO_POL_REG = 0x%x\n", readl(myio.tcon + 0x0088));
    printk("LCD_IO_TRI_REG = 0x%x\n", readl(myio.tcon + 0x008c));
    printk("LCD_CEU_CTL_REG = 0x%x\n", readl(myio.tcon + 0x0100));
    printk("LCD_CPU_TRI0_REG = 0x%x\n", readl(myio.tcon + 0x0160));
    printk("LCD_CPU_TRI1_REG = 0x%x\n", readl(myio.tcon + 0x0164));
    printk("LCD_CPU_TRI2_REG = 0x%x\n", readl(myio.tcon + 0x0168));
    printk("LCD_CPU_TRI3_REG = 0x%x\n", readl(myio.tcon + 0x016c));
    printk("LCD_CPU_TRI4_REG = 0x%x\n", readl(myio.tcon + 0x0170));
    printk("LCD_CPU_TRI5_REG = 0x%x\n", readl(myio.tcon + 0x0174));
    printk("LCD_SYNC_CTL_REG = 0x%x\n", readl(myio.tcon + 0x0230));
    printk("LCD_SYNC_POS_REG = 0x%x\n", readl(myio.tcon + 0x0234));

    printk("PD_CFG0 = 0x%x\n", readl(myio.gpio + 0x0090));
    printk("PD_CFG1 = 0x%x\n", readl(myio.gpio + 0x0094));
    printk("PD_CFG2 = 0x%x\n", readl(myio.gpio + 0x0098));
    printk("======\n");

#if 1
    {
        int i = 0;

        for (i = 0; i < 320 * 240; i++) {
            tcon_write(1, 0x7e0);
        }
    }
#endif

    return 0;
}

static void lcd_reset(void)
{
    uint32_t r = 0;

    r = readl(myio.gpio + PD_DAT);
    r &= ~LCD_RST;
    writel(r, myio.gpio + PD_DAT);
    mdelay(150);

    r |= LCD_RST | LCD_BL;
    writel(r, myio.gpio + PD_DAT);
    mdelay(150);
}

static void gpio_init(void)
{
    uint32_t r = 0; 

    r = readl(myio.gpio + PD_CFG0);
    r = 0x22222221;
    writel(r, myio.gpio + PD_CFG0);

    r = readl(myio.gpio + PD_CFG1);
    r &= 0x000000f0;
    r |= 0x22222202;
    writel(r, myio.gpio + PD_CFG1);

    r = readl(myio.gpio + PD_CFG2);
    r &= 0xfff000000;
    r |= 0x000222222;
    writel(r, myio.gpio + PD_CFG2);

    r = readl(myio.gpio + PD_PULL1);
    r &= 0xfffff0ff;
    r |= 0x00000500;
    writel(r, myio.gpio + PD_PULL1);
}

static int lcd_init(void)
{
    writel(0x11111111, myio.gpio + PD_CFG0);
    writel(0x11111111, myio.gpio + PD_CFG1);
    writel(0x11111111, myio.gpio + PD_CFG2);
    writel(0xffffffff, myio.gpio + PD_DAT);

    lcd_reset();
    lcd_write_cmd(0xb2);
    lcd_write_dat(0x5c);
    lcd_write_dat(0x5c);
    lcd_write_dat(0x00);
    lcd_write_dat(0x33);
    lcd_write_dat(0x33);

    lcd_write_cmd(0xb7);
    lcd_write_dat(0x35);

    lcd_write_cmd(0x21);

    lcd_write_cmd(0x11);

    mdelay(150);

    lcd_write_cmd(0xe0);
    lcd_write_dat(0xd0);
    lcd_write_dat(0x06);
    lcd_write_dat(0x0b);
    lcd_write_dat(0x07);
    lcd_write_dat(0x07);
    lcd_write_dat(0x24);
    lcd_write_dat(0x2e);
    lcd_write_dat(0x32);
    lcd_write_dat(0x46);
    lcd_write_dat(0x37);
    lcd_write_dat(0x13);
    lcd_write_dat(0x13);
    lcd_write_dat(0x2d);
    lcd_write_dat(0x33);

    lcd_write_cmd(0xe1);
    lcd_write_dat(0xd0);
    lcd_write_dat(0x02);
    lcd_write_dat(0x06);
    lcd_write_dat(0x09);
    lcd_write_dat(0x08);
    lcd_write_dat(0x05);
    lcd_write_dat(0x29);
    lcd_write_dat(0x44);
    lcd_write_dat(0x42);
    lcd_write_dat(0x38);
    lcd_write_dat(0x14);
    lcd_write_dat(0x14);
    lcd_write_dat(0x2a);
    lcd_write_dat(0x30);

    lcd_write_cmd(0x36);
    lcd_write_dat(0xb0);

    lcd_write_cmd(0x2a);
    lcd_write_dat(0x00);
    lcd_write_dat(0x00);
    lcd_write_dat(0x01);
    lcd_write_dat(0x3f);

    lcd_write_cmd(0x2b);
    lcd_write_dat(0x00);
    lcd_write_dat(0x00);
    lcd_write_dat(0x00);
    lcd_write_dat(0xef);

    lcd_write_cmd(0x3a);
    lcd_write_dat(0x55);

    lcd_write_cmd(0x29);
    lcd_write_cmd(0x2c);

    return 0;
}

static int myfb_setcolreg(unsigned n, unsigned r, unsigned g, unsigned b, unsigned t, struct fb_info *info)
{
    #define CNVT_TOHW(val, width) ((((val) << (width)) + 0x7FFF - (val)) >> 16)

    if (n >= 16) {
        return -EINVAL;
    }

    r = CNVT_TOHW(r, info->var.red.length);
    b = CNVT_TOHW(b, info->var.blue.length);
    g = CNVT_TOHW(g, info->var.green.length);

    ((u32 *)(info->pseudo_palette))[n] =
        (r << info->var.red.offset) |
        (g << info->var.green.offset) |
        (b << info->var.blue.offset);

    return 0;
}

static int myfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
    struct myfb_par *par = info->par;
    unsigned int line_size = var->xres_virtual * 2;

    if ((var->xres != 320) || (var->yres != 240) || (var->bits_per_pixel != 16)) {
        return -EINVAL;
    }

    var->transp.offset = 0;
    var->transp.length = 0;
    var->red.offset = 11;
    var->red.length = 5;
    var->green.offset = 5;
    var->green.length = 6;
    var->blue.offset = 0;
    var->blue.length = 5;
    var->red.msb_right = 0;
    var->green.msb_right = 0;
    var->blue.msb_right = 0;
    var->transp.msb_right = 0;

    if (line_size * var->yres_virtual > par->vram_size) {
        var->yres_virtual = par->vram_size / line_size;
    }
    if (var->yres > var->yres_virtual) {
        var->yres = var->yres_virtual;
    }
    if (var->xres > var->xres_virtual) {
        var->xres = var->xres_virtual;
    }
    if (var->xres + var->xoffset > var->xres_virtual) {
        var->xoffset = var->xres_virtual - var->xres;
    }
    if (var->yres + var->yoffset > var->yres_virtual) {
        var->yoffset = var->yres_virtual - var->yres;
    }

    return 0;
}

static int myfb_set_par(struct fb_info *info)
{
    struct myfb_par *par = info->par;

    fb_var_to_videomode(&par->mode, &info->var);
    info->fix.visual = FB_VISUAL_TRUECOLOR;
    info->fix.line_length = 320 * 2;

    return 0;
}

static int myfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
    info->var.xoffset = var->xoffset;
    info->var.yoffset = var->yoffset;

    return 0;
}

static int myfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    struct myfb_par *par = info->par;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long size = vma->vm_end - vma->vm_start;

    if (offset >= par->vram_size) {
        return -EINVAL;
    }

    if (size > par->vram_size - offset) {
        return -EINVAL;
    }

    return dma_mmap_coherent(info->device,
        vma,
        par->vram_virt,
        par->vram_phys,
        par->vram_size
    );
}

static int myfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
    return 0;
}

static struct fb_ops myfb_ops = {
    .owner = THIS_MODULE,
    .fb_set_par = myfb_set_par,
    .fb_setcolreg = myfb_setcolreg,
    .fb_check_var = myfb_check_var,
    .fb_pan_display = myfb_pan_display,
    .fb_ioctl = myfb_ioctl,
    .fb_mmap = myfb_mmap,

    .fb_fillrect = sys_fillrect,
    .fb_copyarea = sys_copyarea,
    .fb_imageblit = sys_imageblit,
};

static struct timer_list my_timer;

static void timer_handler(struct timer_list *timer)
{
    if (mypar && mypar->vram_virt) {
        int i = 0;
        uint16_t *p = mypar->vram_virt;

        for (i = 0; i < 320 * 240; i++) {
            tcon_write(1, *p++);
        }
    }

    mod_timer(&my_timer, jiffies + msecs_to_jiffies(1000));
}

static int myfb_probe(struct platform_device *device)
{
    int ret = 0;
    struct fb_info *info = NULL;
    struct myfb_par *par = NULL;
    struct fb_videomode *mode = NULL;

    mode = devm_kzalloc(&device->dev, sizeof(struct fb_videomode), GFP_KERNEL);
    if (mode == NULL) {
        return -ENOMEM;
    }
    mode->name = "320x240";
    mode->xres = 320;
    mode->yres = 240;
    mode->vmode = FB_VMODE_NONINTERLACED;

    info = framebuffer_alloc(sizeof(struct myfb_par), &device->dev);
    if (!info) {
        return -ENOMEM;
    }

    par = info->par;
    par->pdev = device;
    par->dev = &device->dev;
    par->bpp = 16;
    fb_videomode_to_var(&myfb_var, mode);

    par->vram_size = 320 * 240 * 2 * 4;
    par->vram_virt = dma_alloc_coherent(&device->dev, 
        par->vram_size,
        &par->vram_phys,
        GFP_KERNEL | GFP_DMA
    );

    if (!par->vram_virt) {
        return -EINVAL;
    }

    info->screen_base = (char __iomem *)par->vram_virt;
    myfb_fix.smem_start = par->vram_phys;
    myfb_fix.smem_len = par->vram_size;
    myfb_fix.line_length = 320 * 2;

    par->v_palette_base = dma_alloc_coherent(&device->dev,
        PALETTE_SIZE,
        &par->p_palette_base,
        GFP_KERNEL | GFP_DMA
    );

    if (!par->v_palette_base) {
        return -EINVAL;
    }
    memset(par->v_palette_base, 0, PALETTE_SIZE);
    myfb_var.grayscale = 0;
    myfb_var.bits_per_pixel = par->bpp;
    myfb_var.xres = 320;
    myfb_var.yres = 240;
    myfb_var.xres_virtual = 320;
    myfb_var.yres_virtual = 240 * 4;
    myfb_var.bits_per_pixel = 16;

    info->flags = FBINFO_FLAG_DEFAULT;
    info->fix = myfb_fix;
    info->var = myfb_var;
    info->fbops = &myfb_ops;
    info->pseudo_palette = par->pseudo_palette;
    info->fix.visual = (info->var.bits_per_pixel <= 8) ?
        FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
    ret = fb_alloc_cmap(&info->cmap, 256, 0);
    if (ret) {
        return -EINVAL;
    }
    info->cmap.len = 32;

    mypar = par;
    myfb_var.activate = FB_ACTIVATE_FORCE;
    fb_set_var(info, &myfb_var);
    dev_set_drvdata(&device->dev, info);
    if (register_framebuffer(info) < 0) {
        return -EINVAL;
    }

    lcd_init();
    gpio_init();
    tcon_init();

    timer_setup(&my_timer, timer_handler, 0);
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(1000));

    return 0;
}

static int myfb_remove(struct platform_device *dev)
{
    struct fb_info *info = dev_get_drvdata(&dev->dev);
    struct myfb_par *par = info->par;

    if (info) {
        del_timer_sync(&my_timer);
        mypar = NULL;

        unregister_framebuffer(info);
        fb_dealloc_cmap(&info->cmap);

        if (par->v_palette_base) {
            dma_free_coherent(&dev->dev, PALETTE_SIZE, par->v_palette_base, par->p_palette_base);
        }

        if (par->vram_virt) {
            dma_free_coherent(&dev->dev, par->vram_size, par->vram_virt, par->vram_phys);
        }
        framebuffer_release(info);
    }
    iounmap(myio.gpio);

    return 0;
}

static int myfb_suspend(struct platform_device *dev, pm_message_t state)
{
    struct fb_info *info = platform_get_drvdata(dev);

    console_lock();
    fb_set_suspend(info, 1);
    console_unlock();

    return 0;
}

static int myfb_resume(struct platform_device *dev)
{
    struct fb_info *info = platform_get_drvdata(dev);

    console_lock();
    fb_set_suspend(info, 0);
    console_unlock();

    return 0;
}

static const struct of_device_id fb_of_match[] = { { .compatible = "allwinner,sunxi-disp" }, {} };
MODULE_DEVICE_TABLE(of, fb_of_match);

static struct platform_driver fb_driver = {
    .probe = myfb_probe,
    .resume = myfb_resume,
    .remove = myfb_remove,
    .suspend = myfb_suspend,
    .driver = {
        .name   = DRIVER_NAME,
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(fb_of_match),
    },
};

static void sunxi_ioremap(void)
{
    myio.ccu = (uint8_t *)ioremap(CCU_BASE, 4096);
    myio.gpio = (uint8_t *)ioremap(GPIO_BASE, 4096);
    myio.tcon = (uint8_t *)ioremap(TCON_BASE, 4096);
}

static void sunxi_iounmap(void)
{
    iounmap(myio.ccu);
    iounmap(myio.gpio);
    iounmap(myio.tcon);
}

static int __init myfb_init(void)
{
    sunxi_ioremap();
    return platform_driver_register(&fb_driver);
}

static void __exit myfb_cleanup(void)
{
    platform_driver_unregister(&fb_driver);
    sunxi_iounmap();
}

module_init(myfb_init);
module_exit(myfb_cleanup);

MODULE_DESCRIPTION("Framebuffer driver for Gaviar handheld");
MODULE_AUTHOR("Steward Fu <steward.fu@gmail.com>");
MODULE_LICENSE("GPL");

