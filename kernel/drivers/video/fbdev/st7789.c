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
#define LCDC_BASE           0x05461000
#define PLL_CPU_CTRL_REG    0x0000
#define PLL_PERI_CTRL_REG   0x0020
#define PLL_VIDEO0_CTRL_REG 0x0040
#define PLL_VIDEO1_CTRL_REG 0x0048
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
    uint8_t *lcdc;
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

static void lcd_write(uint32_t ctl, uint32_t dat)
{
    uint32_t r = ctl | ((dat & 0xff) << 1) | ((dat & 0xff00) << 2);

    writel(r, myio.gpio + PD_DAT);
    r |= LCD_WR;
    writel(r, myio.gpio + PD_DAT);
}

static void lcd_write_cmd(uint32_t cmd)
{
    lcd_write(LCD_RD | LCD_BL | LCD_RST, cmd);
}

static void lcd_write_dat(uint32_t dat)
{
    lcd_write(LCD_RS | LCD_RD | LCD_BL | LCD_RST, dat);
}

static void lcd_reset(void)
{
    uint32_t r = 0;

    r = readl(myio.gpio + PD_DAT);
    r &= ~LCD_RST;
    writel(r, myio.gpio + PD_DAT);
    mdelay(150);

    r |= LCD_RST;
    writel(r, myio.gpio + PD_DAT);
    mdelay(150);
}

static int lcd_init(struct myfb_par *mypar)
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
            lcd_write_dat(*p++);
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

    myfb_var.activate = FB_ACTIVATE_FORCE;
    fb_set_var(info, &myfb_var);
    dev_set_drvdata(&device->dev, info);
    if (register_framebuffer(info) < 0) {
        return -EINVAL;
    }
    lcd_init(par);

    mypar = par;
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
    myio.lcdc = (uint8_t *)ioremap(LCDC_BASE, 4096);

    printk("%s\n", __func__);

    writel((1 << 31) | (1 << 24) | (1 << 16) | (1 << 0), myio.ccu + G2D_CLK_REG);
    writel((1 << 16) | (1 << 0), myio.ccu + G2D_BGR_REG);

    writel((1 << 31) | (1 << 24), myio.ccu + TCONLCD_CLK_REG);
    writel((1 << 16) | (1 << 0), myio.ccu + TCONLCD_BGR_REG);

    writel((1 << 16) | (1 << 0), myio.ccu + HSTIMER_BGR_REG);


    printk("======\n");
    printk("PLL_CPU_CTRL = 0x%x\n", readl(myio.ccu + PLL_CPU_CTRL_REG));
    printk("PLL_PERI_CTRL = 0x%x\n", readl(myio.ccu + PLL_PERI_CTRL_REG));
    printk("PLL_VIDEO0_CTRL = 0x%x\n", readl(myio.ccu + PLL_VIDEO0_CTRL_REG));
    printk("PLL_VIDEO1_CTRL = 0x%x\n", readl(myio.ccu + PLL_VIDEO1_CTRL_REG));
    printk("DRAM_BGR_REG = 0x%x\n", readl(myio.ccu + DRAM_BGR_REG));
    printk("G2D_CLK_REG = 0x%x\n", readl(myio.ccu + G2D_CLK_REG));
    printk("G2D_BGR_REG = 0x%x\n", readl(myio.ccu + G2D_BGR_REG));
    printk("HSTIMER_BGR_REG = 0x%x\n", readl(myio.ccu + HSTIMER_BGR_REG));
    printk("TCONLCD_CLK_REG = 0x%x\n", readl(myio.ccu + TCONLCD_CLK_REG));
    printk("TCONLCD_BGR_REG = 0x%x\n", readl(myio.ccu + TCONLCD_BGR_REG));
    printk("RISC_CLK_REG = 0x%x\n", readl(myio.ccu + RISC_CLK_REG));
    printk("RISC_GATING_REG = 0x%x\n", readl(myio.ccu + RISC_GATING_REG));
    printk("RISC_CFG_BGR_REG = 0x%x\n", readl(myio.ccu + RISC_CFG_BGR_REG));
    printk("======\n");

/*
#define PLL_CPU_REG         0x0000
#define PLL_PERI_REG        0x0020
#define PLL_VIDEO0_REG      0x0040
#define PLL_VIDEO1_REG      0x0048
#define DRAM_BGR_REG        0x080C
#define G2D_CLK_REG         0x0630
#define G2D_BGR_REG         0x063C
#define TCONLCD_CLK_REG     0x0B60
#define TCONLCD_BGR_REG     0x0B7C
#define RISC_CLK_REG        0x0D00
#define RISC_GATING_REG     0x0D04
#define RISC_CFG_BGR_REG    0x0D0C
*/
}

static void sunxi_iounmap(void)
{
    iounmap(myio.ccu);
    iounmap(myio.gpio);
    iounmap(myio.lcdc);
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

MODULE_DESCRIPTION("framebuffer driver for Gaviar handheld");
MODULE_AUTHOR("Steward Fu <steward.fu@gmail.com>");
MODULE_LICENSE("GPL");

