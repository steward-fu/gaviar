/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <xboot.h>
#include <dma/dma.h>
#include <clk/clk.h>
#include <reset/reset.h>
#include <gpio/gpio.h>
#include <led/led.h>
#include <framebuffer/framebuffer.h>
#include <f133-gpio.h>
#include <f133/reg-de.h>
#include <f133/reg-tconlcd.h>

#define LCD_RST (1 << 0)
#define LCD_WR  (1 << 18)
#define LCD_RS  (1 << 19)
#define LCD_RD  (1 << 20)
#define LCD_BL  (1 << 22)

struct fb_f133_i80_pdata_t {
    virtual_addr_t virt_de;
    virtual_addr_t virt_gpio;
    virtual_addr_t virt_tconlcd;

    char *clk_de;
    char *clk_tconlcd;
    int rst_de;
    int rst_tconlcd;
    int width;
    int height;
    int pwidth;
    int pheight;
    int bits_per_pixel;
    int bytes_per_pixel;
    int pixlen;
    int index;
    void *vram[2];
    struct region_list_t *nrl, *orl;

    struct {
        int pixel_clock_hz;
        int h_front_porch;
        int h_back_porch;
        int h_sync_len;
        int v_front_porch;
        int v_back_porch;
        int v_sync_len;
        int h_sync_active;
        int v_sync_active;
        int den_active;
        int clk_active;
    } timing;

    struct led_t *backlight;
    int brightness;
};

static void tcon_write(struct fb_f133_i80_pdata_t *pdat, uint32_t is_data, uint32_t val)
{
    uint32_t r = 0;
    struct f133_tconlcd_reg_t *tcon = (struct f133_tconlcd_reg_t *)pdat->virt_tconlcd;

    r = read32((virtual_addr_t)&tcon->cpu_intf);
    if (is_data) {
        r |= (1 << 25);
    }
    else {
        r &= ~(1 << 25);
    }
    write32((virtual_addr_t)&tcon->cpu_intf, r);

    while (read32((virtual_addr_t)&tcon->cpu_intf) & 0x00c00000) {
        ndelay(10);
    }

    write32((virtual_addr_t)&tcon->cpu_wr,
        ((val & 0xfc00) << 8) |
        ((val & 0x0300) << 6) |
        ((val & 0x00e0) << 5) |
        ((val & 0x001f) << 3)
    );
}

static void gpio_write(struct fb_f133_i80_pdata_t *pdat, uint32_t is_data, uint32_t val)
{
    uint32_t r = 0;

    r = (val & 0x00ff) << 1;
    r |= (val & 0xff00) << 2;
    r |= is_data ? LCD_RS : 0;
    r |= LCD_RST | LCD_RD | LCD_BL;
    write32(pdat->virt_gpio + F133_PD_DAT, r);

    r |= LCD_WR;
    write32(pdat->virt_gpio + F133_PD_DAT, r);
}

static void lcd_write_cmd(struct fb_f133_i80_pdata_t *pdat, uint32_t val)
{
    gpio_write(pdat, 0, val);
}

static void lcd_write_dat(struct fb_f133_i80_pdata_t *pdat, uint32_t val)
{
    gpio_write(pdat, 1, val);
}

static void init_lcd(struct fb_f133_i80_pdata_t *pdat)
{
    int cc = 0;

    LOG("%s\r\n", __func__);

    gpio_set_value(F133_GPIOD0, 1);
    mdelay(50);
    gpio_set_value(F133_GPIOD0, 0);
    mdelay(150);
    gpio_set_value(F133_GPIOD0, 1);
    mdelay(50);

    gpio_set_value(F133_GPIOD22, 1);

    lcd_write_cmd(pdat, 0xb2);
    lcd_write_dat(pdat, 0x5c);
    lcd_write_dat(pdat, 0x5c);
    lcd_write_dat(pdat, 0x00);
    lcd_write_dat(pdat, 0x33);
    lcd_write_dat(pdat, 0x33);

    lcd_write_cmd(pdat, 0xb7);
    lcd_write_dat(pdat, 0x35);

    lcd_write_cmd(pdat, 0x21);

    lcd_write_cmd(pdat, 0x11);

    mdelay(150);

    lcd_write_cmd(pdat, 0xe0);
    lcd_write_dat(pdat, 0xd0);
    lcd_write_dat(pdat, 0x06);
    lcd_write_dat(pdat, 0x0b);
    lcd_write_dat(pdat, 0x07);
    lcd_write_dat(pdat, 0x07);
    lcd_write_dat(pdat, 0x24);
    lcd_write_dat(pdat, 0x2e);
    lcd_write_dat(pdat, 0x32);
    lcd_write_dat(pdat, 0x46);
    lcd_write_dat(pdat, 0x37);
    lcd_write_dat(pdat, 0x13);
    lcd_write_dat(pdat, 0x13);
    lcd_write_dat(pdat, 0x2d);
    lcd_write_dat(pdat, 0x33);

    lcd_write_cmd(pdat, 0xe1);
    lcd_write_dat(pdat, 0xd0);
    lcd_write_dat(pdat, 0x02);
    lcd_write_dat(pdat, 0x06);
    lcd_write_dat(pdat, 0x09);
    lcd_write_dat(pdat, 0x08);
    lcd_write_dat(pdat, 0x05);
    lcd_write_dat(pdat, 0x29);
    lcd_write_dat(pdat, 0x44);
    lcd_write_dat(pdat, 0x42);
    lcd_write_dat(pdat, 0x38);
    lcd_write_dat(pdat, 0x14);
    lcd_write_dat(pdat, 0x14);
    lcd_write_dat(pdat, 0x2a);
    lcd_write_dat(pdat, 0x30);

    lcd_write_cmd(pdat, 0x36);
    lcd_write_dat(pdat, 0xb0);

    lcd_write_cmd(pdat, 0x2a);
    lcd_write_dat(pdat, 0x00);
    lcd_write_dat(pdat, 0x00);
    lcd_write_dat(pdat, 0x01);
    lcd_write_dat(pdat, 0x3f);

    lcd_write_cmd(pdat, 0x2b);
    lcd_write_dat(pdat, 0x00);
    lcd_write_dat(pdat, 0x00);
    lcd_write_dat(pdat, 0x00);
    lcd_write_dat(pdat, 0xef);

    lcd_write_cmd(pdat, 0x3a);
    lcd_write_dat(pdat, 0x55);

    lcd_write_cmd(pdat, 0x29);
    lcd_write_cmd(pdat, 0x2c);

#if 1
    for (cc = 0; cc < 320 * 240; cc++) {
        lcd_write_dat(pdat, 0);
    }
#endif
}

static void inline f133_de_enable(struct fb_f133_i80_pdata_t *pdat)
{
    struct de_glb_t *glb = (struct de_glb_t *)(pdat->virt_de + F133_DE_MUX_GLB);

    LOG("%s\r\n", __func__);
    write32((virtual_addr_t)&glb->dbuff, 1);
}

static inline void f133_de_set_address(struct fb_f133_i80_pdata_t *pdat, void *vram)
{
    struct de_ui_t *ui = (struct de_ui_t *)(pdat->virt_de + F133_DE_MUX_CHAN + 0x1000 * 1);

    LOG("%s\r\n", __func__);
    write32((virtual_addr_t)&ui->cfg[0].top_laddr, (u32_t)(unsigned long)vram);
}

static inline void f133_de_set_mode(struct fb_f133_i80_pdata_t *pdat)
{
    struct de_clk_t *clk = (struct de_clk_t *)(pdat->virt_de);
    struct de_glb_t *glb = (struct de_glb_t *)(pdat->virt_de + F133_DE_MUX_GLB);
    struct de_bld_t *bld = (struct de_bld_t *)(pdat->virt_de + F133_DE_MUX_BLD);
    struct de_ui_t *ui = (struct de_ui_t *)(pdat->virt_de + F133_DE_MUX_CHAN + 0x1000 * 1);
    u32_t size = (((pdat->height - 1) << 16) | (pdat->width - 1));
    u32_t val;
    int i;

    LOG("%s\r\n", __func__);

    val = read32((virtual_addr_t)&clk->rst_cfg);
    val |= 1 << 0;
    write32((virtual_addr_t)&clk->rst_cfg, val);

    val = read32((virtual_addr_t)&clk->gate_cfg);
    val |= 1 << 0;
    write32((virtual_addr_t)&clk->gate_cfg, val);

    val = read32((virtual_addr_t)&clk->bus_cfg);
    val |= 1 << 0;
    write32((virtual_addr_t)&clk->bus_cfg, val);

    val = read32((virtual_addr_t)&clk->sel_cfg);
    val &= ~(1 << 0);
    write32((virtual_addr_t)&clk->sel_cfg, val);

    write32((virtual_addr_t)&glb->ctl, (1 << 0));
    write32((virtual_addr_t)&glb->status, 0);
    write32((virtual_addr_t)&glb->dbuff, 1);
    write32((virtual_addr_t)&glb->size, size);

    for (i = 0; i < 4; i++) {
        void *chan = (void *)(pdat->virt_de + F133_DE_MUX_CHAN + 0x1000 * i);
        memset(chan, 0, i == 0 ? sizeof(struct de_vi_t) : sizeof(struct de_ui_t));
    }
    memset(bld, 0, sizeof(struct de_bld_t));

    write32((virtual_addr_t)&bld->fcolor_ctl, 0x00000101);
    write32((virtual_addr_t)&bld->route, 1);
    write32((virtual_addr_t)&bld->premultiply, 0);
    write32((virtual_addr_t)&bld->bkcolor, 0xff000000);
    write32((virtual_addr_t)&bld->bld_mode[0], 0x03010301);
    write32((virtual_addr_t)&bld->bld_mode[1], 0x03010301);
    write32((virtual_addr_t)&bld->output_size, size);
    write32((virtual_addr_t)&bld->out_ctl, 0);
    write32((virtual_addr_t)&bld->ck_ctl, 0);
    for (i = 0; i < 4; i++) {
        write32((virtual_addr_t)&bld->attr[i].fcolor, 0xff000000);
        write32((virtual_addr_t)&bld->attr[i].insize, size);
    }

    write32(pdat->virt_de + F133_DE_MUX_VSU, 0);
    write32(pdat->virt_de + F133_DE_MUX_GSU1, 0);
    write32(pdat->virt_de + F133_DE_MUX_GSU2, 0);
    write32(pdat->virt_de + F133_DE_MUX_GSU3, 0);
    write32(pdat->virt_de + F133_DE_MUX_FCE, 0);
    write32(pdat->virt_de + F133_DE_MUX_BWS, 0);
    write32(pdat->virt_de + F133_DE_MUX_LTI, 0);
    write32(pdat->virt_de + F133_DE_MUX_PEAK, 0);
    write32(pdat->virt_de + F133_DE_MUX_ASE, 0);
    write32(pdat->virt_de + F133_DE_MUX_FCC, 0);
    write32(pdat->virt_de + F133_DE_MUX_DCSC, 0);

    write32((virtual_addr_t)&ui->cfg[0].attr, (1 << 0) | (4 << 8) | (1 << 1) | (0xff << 24));
    write32((virtual_addr_t)&ui->cfg[0].size, size);
    write32((virtual_addr_t)&ui->cfg[0].coord, 0);
    write32((virtual_addr_t)&ui->cfg[0].pitch, 4 * pdat->width);
    write32((virtual_addr_t)&ui->cfg[0].top_laddr, (u32_t)(unsigned long)pdat->vram[pdat->index]);
    write32((virtual_addr_t)&ui->ovl_size, size);
}

static void f133_tconlcd_enable(struct fb_f133_i80_pdata_t *pdat)
{
    struct f133_tconlcd_reg_t *tcon = (struct f133_tconlcd_reg_t *)pdat->virt_tconlcd;
    u32_t val;

    LOG("%s\r\n", __func__);
    val = read32((virtual_addr_t)&tcon->gctrl);
    val |= (1 << 31);
    write32((virtual_addr_t)&tcon->gctrl, val);
}

static void f133_tconlcd_disable(struct fb_f133_i80_pdata_t *pdat)
{
    struct f133_tconlcd_reg_t *tcon = (struct f133_tconlcd_reg_t *)pdat->virt_tconlcd;
    u32_t val;

    LOG("%s\r\n", __func__);
    val = read32((virtual_addr_t)&tcon->dclk);
    val &= ~(0xf << 28);
    write32((virtual_addr_t)&tcon->dclk, val);

    write32((virtual_addr_t)&tcon->gctrl, 0);
    write32((virtual_addr_t)&tcon->gint0, 0);
}

static void f133_tconlcd_set_timing(struct fb_f133_i80_pdata_t *pdat)
{
    struct f133_tconlcd_reg_t *tcon = (struct f133_tconlcd_reg_t *)pdat->virt_tconlcd;

    LOG("%s\r\n", __func__);
    write32((virtual_addr_t)&tcon->gctrl, 0x80000000);
    write32((virtual_addr_t)&tcon->gint0, 0x0);
    write32((virtual_addr_t)&tcon->gint1, 0x0);
    write32((virtual_addr_t)&tcon->frm_ctrl, 0x80000050);
    write32((virtual_addr_t)&tcon->frm_seed[0], 0x1);
    write32((virtual_addr_t)&tcon->frm_seed[1], 0x3);
    write32((virtual_addr_t)&tcon->frm_seed[2], 0x5);
    write32((virtual_addr_t)&tcon->frm_seed[3], 0x7);
    write32((virtual_addr_t)&tcon->frm_seed[4], 0xb);
    write32((virtual_addr_t)&tcon->frm_seed[5], 0xd);
    write32((virtual_addr_t)&tcon->frm_table[0], 0x1010000);
    write32((virtual_addr_t)&tcon->frm_table[1], 0x15151111);
    write32((virtual_addr_t)&tcon->frm_table[2], 0x57575555);
    write32((virtual_addr_t)&tcon->frm_table[3], 0x7f7f7777);
    write32((virtual_addr_t)&tcon->fifo_3d, 0x0);
    write32((virtual_addr_t)&tcon->ctrl, 0x81000000);
    write32((virtual_addr_t)&tcon->dclk, 0xf0000030);
    write32((virtual_addr_t)&tcon->timing0, 0x13f00ef);
    write32((virtual_addr_t)&tcon->timing1, 0x0);
    write32((virtual_addr_t)&tcon->timing2, 0x0);
    write32((virtual_addr_t)&tcon->timing3, 0x0);
    write32((virtual_addr_t)&tcon->hv_intf, 0x0);
    write32((virtual_addr_t)&tcon->cpu_intf, 0x86000005);
    write32((virtual_addr_t)&tcon->lvds_intf, 0x0);
    write32((virtual_addr_t)&tcon->io_polarity, 0x0);
    write32((virtual_addr_t)&tcon->io_tristate, 0x0);
    write32((virtual_addr_t)&tcon->ceu_ctl, 0x0);
    write32((virtual_addr_t)&tcon->cpu_tri0, 0x3f013f);
    write32((virtual_addr_t)&tcon->cpu_tri1, 0xef);
    write32((virtual_addr_t)&tcon->cpu_tri2, 0x6720013f);
    write32((virtual_addr_t)&tcon->cpu_tri3, 0x10617f00);
    write32((virtual_addr_t)&tcon->cpu_tri4, 0x0);
    write32((virtual_addr_t)&tcon->cpu_tri5, 0x0);
    write32((virtual_addr_t)&tcon->sync_ctl, 0x0);
    write32((virtual_addr_t)&tcon->sync_pos, 0x0);
}

static void f133_tconlcd_set_dither(struct fb_f133_i80_pdata_t *pdat)
{
    struct f133_tconlcd_reg_t *tcon = (struct f133_tconlcd_reg_t *)pdat->virt_tconlcd;

    LOG("%s\r\n", __func__);
    if ((pdat->bits_per_pixel == 16) || (pdat->bits_per_pixel == 18)) {
        write32((virtual_addr_t)&tcon->frm_seed[0], 0x11111111);
        write32((virtual_addr_t)&tcon->frm_seed[1], 0x11111111);
        write32((virtual_addr_t)&tcon->frm_seed[2], 0x11111111);
        write32((virtual_addr_t)&tcon->frm_seed[3], 0x11111111);
        write32((virtual_addr_t)&tcon->frm_seed[4], 0x11111111);
        write32((virtual_addr_t)&tcon->frm_seed[5], 0x11111111);
        write32((virtual_addr_t)&tcon->frm_table[0], 0x01010000);
        write32((virtual_addr_t)&tcon->frm_table[1], 0x15151111);
        write32((virtual_addr_t)&tcon->frm_table[2], 0x57575555);
        write32((virtual_addr_t)&tcon->frm_table[3], 0x7f7f7777);

        if (pdat->bits_per_pixel == 16) {
            write32((virtual_addr_t)&tcon->frm_ctrl, (1 << 31) | (1 << 6) | (0 << 5)| (1 << 4));
        }
        else if (pdat->bits_per_pixel == 18) {
            write32((virtual_addr_t)&tcon->frm_ctrl, (1 << 31) | (0 << 6) | (0 << 5)| (0 << 4));
        }
    }
}

static void fb_f133_cfg_gpios(int base, int n, int cfg, enum gpio_pull_t pull, enum gpio_drv_t drv)
{
    LOG("%s\r\n", __func__);
    for (; n > 0; n--, base++) {
        gpio_set_cfg(base, cfg);
        gpio_set_pull(base, pull);
        gpio_set_drv(base, drv);
    }
}

static void fb_f133_i80_init(struct fb_f133_i80_pdata_t *pdat)
{
    LOG("%s, pdat->bits_per_pixel=%d\r\n", __func__, pdat->bits_per_pixel);

    fb_f133_cfg_gpios(F133_GPIOD0, 23, 1, GPIO_PULL_NONE, GPIO_DRV_STRONG);
    fb_f133_cfg_gpios(F133_GPIOD22, 1, 1, GPIO_PULL_NONE, GPIO_DRV_STRONG);
    init_lcd(pdat);

    if (pdat->bits_per_pixel == 16) {
        fb_f133_cfg_gpios(F133_GPIOD1, 5, 0x2, GPIO_PULL_NONE, GPIO_DRV_STRONG);
        fb_f133_cfg_gpios(F133_GPIOD6, 6, 0x2, GPIO_PULL_NONE, GPIO_DRV_STRONG);
        fb_f133_cfg_gpios(F133_GPIOD13, 5, 0x2, GPIO_PULL_NONE, GPIO_DRV_STRONG);
        fb_f133_cfg_gpios(F133_GPIOD18, 4, 0x2, GPIO_PULL_NONE, GPIO_DRV_STRONG);
    }

    //f133_tconlcd_disable(pdat);
    f133_tconlcd_set_timing(pdat);
    //f133_tconlcd_set_dither(pdat);
    //f133_tconlcd_enable(pdat);
    //f133_de_set_mode(pdat);
    //f133_de_enable(pdat);
    //f133_de_set_address(pdat, pdat->vram[pdat->index]);
    //f133_de_enable(pdat);

{
    int cc = 0;
    uint16_t *p0 = pdat->vram[0];
    uint16_t *p1 = pdat->vram[1];
    for (cc = 0; cc < 320 * 240; cc++) {
        *p0++ = 0x7e0;
        *p1++ = 0x1f;
        tcon_write(pdat, 1, 0xf800);
    }
}
}

static void fb_setbl(struct framebuffer_t *fb, int brightness)
{
    struct fb_f133_i80_pdata_t *pdat = (struct fb_f133_i80_pdata_t *)fb->priv;

    LOG("%s\r\n", __func__);
    led_set_brightness(pdat->backlight, brightness);
}

static int fb_getbl(struct framebuffer_t *fb)
{
    struct fb_f133_i80_pdata_t *pdat = (struct fb_f133_i80_pdata_t *)fb->priv;

    LOG("%s\r\n", __func__);
    return led_get_brightness(pdat->backlight);
}

static struct surface_t* fb_create(struct framebuffer_t *fb)
{
    struct surface_t *s = NULL;
    struct fb_f133_i80_pdata_t * pdat = (struct fb_f133_i80_pdata_t *)fb->priv;

    LOG("%s\r\n", __func__);
    s = surface_alloc(pdat->width, pdat->height);
    LOG("%s, s = %p\r\n", __func__, s);

    return s;
}

static void fb_destroy(struct framebuffer_t *fb, struct surface_t *s)
{
    LOG("%s, s=%p\r\n", __func__, s);
    if (s) {
        surface_free(s);
    }
}

static void fb_present(struct framebuffer_t *fb, struct surface_t *s, struct region_list_t *rl)
{
    struct fb_f133_i80_pdata_t *pdat = (struct fb_f133_i80_pdata_t *)fb->priv;
    struct region_list_t *nrl = pdat->nrl;

    LOG("%s\r\n", __func__);
    region_list_clear(nrl);
    region_list_merge(nrl, pdat->orl);
    region_list_merge(nrl, rl);
    region_list_clone(pdat->orl, rl);

    pdat->index = (pdat->index + 1) & 0x1;
    if (nrl->count > 0) {
        present_surface(pdat->vram[pdat->index], s, nrl);
    }
    else {
        memcpy(pdat->vram[pdat->index], s->pixels, s->pixlen);
    }
    dma_cache_sync(pdat->vram[pdat->index], pdat->pixlen, DMA_TO_DEVICE);
    f133_de_set_address(pdat, pdat->vram[pdat->index]);
    f133_de_enable(pdat);
}

static struct device_t* fb_f133_i80_probe(struct driver_t *drv, struct dtnode_t *n)
{
    struct fb_f133_i80_pdata_t *pdat;
    struct framebuffer_t *fb;
    struct device_t *dev;
    virtual_addr_t virt = phys_to_virt(dt_read_address(n));
    char *clk_de = dt_read_string(n, "clock-name-de", NULL);
    char *clk_tconlcd = dt_read_string(n, "clock-name-tconlcd", NULL);

    LOG("%s\r\n", __func__);
    if (!search_clk(clk_de) || !search_clk(clk_tconlcd)) {
        return NULL;
    }

    pdat = malloc(sizeof(struct fb_f133_i80_pdata_t));
    if (!pdat) {
        return NULL;
    }

    fb = malloc(sizeof(struct framebuffer_t));
    if (!fb) {
        free(pdat);
        return NULL;
    }

    pdat->virt_de = virt;
    pdat->virt_gpio = phys_to_virt(F133_GPIO_BASE);
    pdat->virt_tconlcd = phys_to_virt(F133_TCONLCD_BASE);
    pdat->clk_de = strdup(clk_de);
    pdat->clk_tconlcd = strdup(clk_tconlcd);
    pdat->rst_de = dt_read_int(n, "reset-de", -1);
    pdat->rst_tconlcd = dt_read_int(n, "reset-tconlcd", -1);
    pdat->width = dt_read_int(n, "width", 320);
    pdat->height = dt_read_int(n, "height", 240);
    pdat->pwidth = dt_read_int(n, "physical-width", 320);
    pdat->pheight = dt_read_int(n, "physical-height", 240);
    pdat->bits_per_pixel = dt_read_int(n, "bits-per-pixel", 16);
    pdat->bytes_per_pixel = 2;
    pdat->pixlen = pdat->width * pdat->height * pdat->bytes_per_pixel;
    pdat->index = 0;
    pdat->vram[0] = dma_alloc_noncoherent(pdat->pixlen);
    pdat->vram[1] = dma_alloc_noncoherent(pdat->pixlen);
    pdat->nrl = region_list_alloc(0);
    pdat->orl = region_list_alloc(0);
    memset(pdat->vram[0], 0, pdat->pixlen);
    memset(pdat->vram[1], 0, pdat->pixlen);

    pdat->timing.pixel_clock_hz = dt_read_long(n, "clock-frequency", 33000000);
    pdat->timing.h_front_porch = dt_read_int(n, "hfront-porch", 40);
    pdat->timing.h_back_porch = dt_read_int(n, "hback-porch", 87);
    pdat->timing.h_sync_len = dt_read_int(n, "hsync-len", 1);
    pdat->timing.v_front_porch = dt_read_int(n, "vfront-porch", 13);
    pdat->timing.v_back_porch = dt_read_int(n, "vback-porch", 31);
    pdat->timing.v_sync_len = dt_read_int(n, "vsync-len", 1);
    pdat->timing.h_sync_active = dt_read_bool(n, "hsync-active", 0);
    pdat->timing.v_sync_active = dt_read_bool(n, "vsync-active", 0);
    pdat->timing.den_active = dt_read_bool(n, "den-active", 0);
    pdat->timing.clk_active = dt_read_bool(n, "clk-active", 0);
    pdat->backlight = search_led(dt_read_string(n, "backlight", NULL));

    fb->name = alloc_device_name(dt_read_name(n), dt_read_id(n));
    fb->width = pdat->width;
    fb->height = pdat->height;
    fb->pwidth = pdat->pwidth;
    fb->pheight = pdat->pheight;
    fb->setbl = fb_setbl;
    fb->getbl = fb_getbl;
    fb->create = fb_create;
    fb->destroy = fb_destroy;
    fb->present = fb_present;
    fb->priv = pdat;

    clk_enable(pdat->clk_de);
    clk_enable(pdat->clk_tconlcd);
    if (pdat->rst_de >= 0) {
        reset_deassert(pdat->rst_de);
    }
    if (pdat->rst_tconlcd >= 0) {
        reset_deassert(pdat->rst_tconlcd);
    }
    fb_f133_i80_init(pdat);

    if (!(dev = register_framebuffer(fb, drv))) {
        clk_disable(pdat->clk_de);
        clk_disable(pdat->clk_tconlcd);
        free(pdat->clk_de);
        free(pdat->clk_tconlcd);
        dma_free_noncoherent(pdat->vram[0]);
        dma_free_noncoherent(pdat->vram[1]);
        region_list_free(pdat->nrl);
        region_list_free(pdat->orl);
        free_device_name(fb->name);
        free(fb->priv);
        free(fb);
        return NULL;
    }
    return dev;
}

static void fb_f133_i80_remove(struct device_t *dev)
{
    struct framebuffer_t *fb = (struct framebuffer_t *)dev->priv;
    struct fb_f133_i80_pdata_t *pdat = (struct fb_f133_i80_pdata_t *)fb->priv;

    LOG("%s\r\n", __func__);
    if (fb) {
        unregister_framebuffer(fb);
        clk_disable(pdat->clk_de);
        clk_disable(pdat->clk_tconlcd);
        free(pdat->clk_de);
        free(pdat->clk_tconlcd);
        dma_free_noncoherent(pdat->vram[0]);
        dma_free_noncoherent(pdat->vram[1]);
        region_list_free(pdat->nrl);
        region_list_free(pdat->orl);
        free_device_name(fb->name);
        free(fb->priv);
        free(fb);
    }
}

static void fb_f133_i80_suspend(struct device_t *dev)
{
    struct framebuffer_t *fb = (struct framebuffer_t *)dev->priv;
    struct fb_f133_i80_pdata_t *pdat = (struct fb_f133_i80_pdata_t *)fb->priv;

    LOG("%s\r\n", __func__);
    pdat->brightness = led_get_brightness(pdat->backlight);
    led_set_brightness(pdat->backlight, 0);
}

static void fb_f133_i80_resume(struct device_t *dev)
{
    struct framebuffer_t *fb = (struct framebuffer_t *)dev->priv;
    struct fb_f133_i80_pdata_t *pdat = (struct fb_f133_i80_pdata_t *)fb->priv;

    LOG("%s\r\n", __func__);
    led_set_brightness(pdat->backlight, pdat->brightness);
}

static struct driver_t fb_f133_i80 = {
    .name = "fb-f133-i80",
    .probe = fb_f133_i80_probe,
    .remove = fb_f133_i80_remove,
    .suspend = fb_f133_i80_suspend,
    .resume = fb_f133_i80_resume,
};

static __init void fb_f133_i80_driver_init(void)
{
    LOG("%s\r\n", __func__);
    register_driver(&fb_f133_i80);
}

static __exit void fb_f133_i80_driver_exit(void)
{
    LOG("%s\r\n", __func__);
    unregister_driver(&fb_f133_i80);
}

driver_initcall(fb_f133_i80_driver_init);
driver_exitcall(fb_f133_i80_driver_exit);
