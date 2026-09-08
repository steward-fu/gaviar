/*
 * drivers/video/fbdev/sunxi/disp2/disp/lcd/d395t9375v0.c
 *
 * Copyright (c) 2007-2018 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
 *         zhangyuanjings <zhangyuanjings@allwinnertech.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *

&lcd0 {
	lcd_used            = <1>;
	lcd_driver_name     = "d395t9375v0";

	lcd_backlight       = <255>;

	lcd_if          	= <0>;
	lcd_hv_if       	= <0>;

	lcd_x           	= <480>;
	lcd_y           	= <480>;
	lcd_width       	= <72>;
	lcd_height      	= <70>;
	lcd_dclk_freq   	= <18>;

	lcd_hbp             = <45>;
	lcd_ht              = <531>;
	lcd_hspw            = <2>;
	lcd_vbp             = <25>;
	lcd_vt              = <507>;
	lcd_vspw            = <10>;

	lcd_pwm_used        = <1>;
	lcd_pwm_ch          = <7>;
	lcd_pwm_freq        = <1000>;
	lcd_pwm_pol         = <0>;
	lcd_pwm_max_limit   = <255>;

	lcd_frm         	= <0>;
	lcd_io_phase    	= <0x0000>;
	lcd_gamma_en    	= <0>;
	lcd_cmap_en     	= <0>;
	lcd_hv_clk_phase	= <0>;
	lcd_hv_sync_polarity= <0>;
	lcd_rb_swap         = <0>;

	deu_mode            = <0>;
	lcdgamma4iep        = <22>;
	smart_color         = <90>;

	lcd_gpio_0 = <&pio PE 8 GPIO_ACTIVE_HIGH>;    RST
	lcd_gpio_1 = <&pio PE 9 GPIO_ACTIVE_HIGH>;    CS
	lcd_gpio_2 = <&pio PE 7 GPIO_ACTIVE_HIGH>;    SDA
	lcd_gpio_3 = <&pio PE 6 GPIO_ACTIVE_HIGH>;    SCK

	pinctrl-0 = <&rgb18_pins_a>;
	pinctrl-1 = <&rgb18_pins_b>;
};
*/

#include "d395t9375v0.h"

#define spi_scl_1 sunxi_lcd_gpio_set_value(0, 3, 1)
#define spi_scl_0 sunxi_lcd_gpio_set_value(0, 3, 0)
#define spi_sdi_1 sunxi_lcd_gpio_set_value(0, 2, 1)
#define spi_sdi_0 sunxi_lcd_gpio_set_value(0, 2, 0)
#define spi_cs_1  sunxi_lcd_gpio_set_value(0, 1, 1)
#define spi_cs_0  sunxi_lcd_gpio_set_value(0, 1, 0)
#define spi_rst_1 sunxi_lcd_gpio_set_value(0, 0, 1)
#define spi_rst_0 sunxi_lcd_gpio_set_value(0, 0, 0)

static void LCD_power_on(u32 sel);
static void LCD_power_off(u32 sel);
static void LCD_bl_open(u32 sel);
static void LCD_bl_close(u32 sel);

static void LCD_panel_init(u32 sel);
static void LCD_panel_exit(u32 sel);

static void LCD_cfg_panel_info(struct panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
	    /* {input value, corrected value} */
	    {0, 0},     {15, 15},   {30, 30},   {45, 45},   {60, 60},
	    {75, 75},   {90, 90},   {105, 105}, {120, 120}, {135, 135},
	    {150, 150}, {165, 165}, {180, 180}, {195, 195}, {210, 210},
	    {225, 225}, {240, 240}, {255, 255},
	};

	u32 lcd_cmap_tbl[2][3][4] = {
	    {
		{LCD_CMAP_G0, LCD_CMAP_B1, LCD_CMAP_G2, LCD_CMAP_B3},
		{LCD_CMAP_B0, LCD_CMAP_R1, LCD_CMAP_B2, LCD_CMAP_R3},
		{LCD_CMAP_R0, LCD_CMAP_G1, LCD_CMAP_R2, LCD_CMAP_G3},
	    },
	    {
		{LCD_CMAP_B3, LCD_CMAP_G2, LCD_CMAP_B1, LCD_CMAP_G0},
		{LCD_CMAP_R3, LCD_CMAP_B2, LCD_CMAP_R1, LCD_CMAP_B0},
		{LCD_CMAP_G3, LCD_CMAP_R2, LCD_CMAP_G1, LCD_CMAP_R0},
	    },
	};

	items = sizeof(lcd_gamma_tbl) / 2;
	for (i = 0; i < items - 1; i++) {
		u32 num = lcd_gamma_tbl[i + 1][0] - lcd_gamma_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value =
			    lcd_gamma_tbl[i][1] +
			    ((lcd_gamma_tbl[i + 1][1] - lcd_gamma_tbl[i][1]) *
			     j) /
				num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
			    (value << 16) + (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items - 1][1] << 16) +
				   (lcd_gamma_tbl[items - 1][1] << 8) +
				   lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));
}

static s32 LCD_open_flow(u32 sel)
{
	/* open lcd power, and delay 50ms */
	LCD_OPEN_FUNC(sel, LCD_power_on, 20);
	/* open lcd power, than delay 200ms */
	LCD_OPEN_FUNC(sel, LCD_panel_init, 20);
	/* open lcd controller, and delay 100ms */
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 100);
	/* open lcd backlight, and delay 0ms */
	LCD_OPEN_FUNC(sel, LCD_bl_open, 0);

	return 0;
}

static s32 LCD_close_flow(u32 sel)
{
	/* close lcd backlight, and delay 0ms */
	LCD_CLOSE_FUNC(sel, LCD_bl_close, 0);
	/* close lcd controller, and delay 0ms */
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	/* open lcd power, than delay 200ms */
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 200);
	/* close lcd power, and delay 500ms */
	LCD_CLOSE_FUNC(sel, LCD_power_off, 500);

	return 0;
}

static void LCD_power_on(u32 sel)
{
	/* config lcd_power pin to open lcd power0 */
	sunxi_lcd_power_enable(sel, 0);
	sunxi_lcd_pin_cfg(sel, 1);
}

static void LCD_power_off(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);
	/* config lcd_power pin to close lcd power0 */
	sunxi_lcd_power_disable(sel, 0);
}

static void LCD_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);
	/* config lcd_bl_en pin to open lcd backlight */
	sunxi_lcd_backlight_enable(sel);
}

static void LCD_bl_close(u32 sel)
{
	/* config lcd_bl_en pin to close lcd backlight */
	sunxi_lcd_backlight_disable(sel);
	sunxi_lcd_pwm_disable(sel);
}

static void d395t9375v0_spi_write_cmd(u32 value)
{
	u32 i;
	spi_cs_0;
	spi_sdi_0;
	spi_scl_0;
	sunxi_lcd_delay_us(10);
	spi_scl_1;
	for (i = 0; i < 8; i++) {
		sunxi_lcd_delay_us(10);
		if (value & 0x80)
			spi_sdi_1;
		else
			spi_sdi_0;
		spi_scl_0;
		sunxi_lcd_delay_us(10);
		spi_scl_1;
		value <<= 1;
	}
	sunxi_lcd_delay_us(10);
	spi_cs_1;
}

static void d395t9375v0_spi_write_data(u32 value)
{
	u32 i;
	spi_cs_0;
	spi_sdi_1;
	spi_scl_0;
	sunxi_lcd_delay_us(10);
	spi_scl_1;
	for (i = 0; i < 8; i++) {
		sunxi_lcd_delay_us(10);
		if (value & 0x80)
			spi_sdi_1;
		else
			spi_sdi_0;
		value <<= 1;
		sunxi_lcd_delay_us(10);
		spi_scl_0;
		spi_scl_1;
	}
	sunxi_lcd_delay_us(10);
	spi_cs_1;
}

static void LCD_panel_init(u32 sel)
{
	/* Reset Panel */
	spi_rst_0;
	sunxi_lcd_delay_ms(120);
	spi_rst_1;
	sunxi_lcd_delay_ms(120);

	d395t9375v0_spi_write_cmd (0xFF);
	d395t9375v0_spi_write_data (0x77);
	d395t9375v0_spi_write_data (0x01);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x13);
	d395t9375v0_spi_write_cmd (0xEF);
	d395t9375v0_spi_write_data (0x08);
	d395t9375v0_spi_write_cmd (0xFF);
	d395t9375v0_spi_write_data (0x77);
	d395t9375v0_spi_write_data (0x01);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x10);
	d395t9375v0_spi_write_cmd (0xC0);
	d395t9375v0_spi_write_data (0x3B);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_cmd (0xC1);
	d395t9375v0_spi_write_data (0x0D);
	d395t9375v0_spi_write_data (0x02);
	d395t9375v0_spi_write_cmd (0xC2);
	d395t9375v0_spi_write_data (0x21);
	d395t9375v0_spi_write_data (0x08);
	d395t9375v0_spi_write_cmd (0xCD);
	d395t9375v0_spi_write_data (0x08);
	d395t9375v0_spi_write_cmd (0xB0);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_data (0x18);
	d395t9375v0_spi_write_data (0x0E);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_data (0x06);
	d395t9375v0_spi_write_data (0x07);
	d395t9375v0_spi_write_data (0x08);
	d395t9375v0_spi_write_data (0x07);
	d395t9375v0_spi_write_data (0x22);
	d395t9375v0_spi_write_data (0x04);
	d395t9375v0_spi_write_data (0x12);
	d395t9375v0_spi_write_data (0x0F);
	d395t9375v0_spi_write_data (0xAA);
	d395t9375v0_spi_write_data (0x31);
	d395t9375v0_spi_write_data (0x18);
	d395t9375v0_spi_write_cmd (0xB1);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_data (0x19);
	d395t9375v0_spi_write_data (0x0E);
	d395t9375v0_spi_write_data (0x12);
	d395t9375v0_spi_write_data (0x07);
	d395t9375v0_spi_write_data (0x08);
	d395t9375v0_spi_write_data (0x08);
	d395t9375v0_spi_write_data (0x08);
	d395t9375v0_spi_write_data (0x22);
	d395t9375v0_spi_write_data (0x04);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_data (0xA9);
	d395t9375v0_spi_write_data (0x32);
	d395t9375v0_spi_write_data (0x18);
	d395t9375v0_spi_write_cmd (0xFF);
	d395t9375v0_spi_write_data (0x77);
	d395t9375v0_spi_write_data (0x01);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_cmd (0xB0);
	d395t9375v0_spi_write_data (0x60);
	d395t9375v0_spi_write_cmd (0xB1);
	d395t9375v0_spi_write_data (0x30);
	d395t9375v0_spi_write_cmd (0xB2);
	d395t9375v0_spi_write_data (0x87);
	d395t9375v0_spi_write_cmd (0xB3);
	d395t9375v0_spi_write_data (0x80);
	d395t9375v0_spi_write_cmd (0xB5);
	d395t9375v0_spi_write_data (0x49);
	d395t9375v0_spi_write_cmd (0xB7);
	d395t9375v0_spi_write_data (0x85);
	d395t9375v0_spi_write_cmd (0xB8);
	d395t9375v0_spi_write_data (0x21);
	d395t9375v0_spi_write_cmd (0xC1);
	d395t9375v0_spi_write_data (0x78);
	d395t9375v0_spi_write_cmd (0xC2);
	d395t9375v0_spi_write_data (0x78);
	sunxi_lcd_delay_ms(20);
	d395t9375v0_spi_write_cmd (0xE0);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x1B);
	d395t9375v0_spi_write_data (0x02);
	d395t9375v0_spi_write_cmd (0xE1);
	d395t9375v0_spi_write_data (0x08);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x07);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x44);
	d395t9375v0_spi_write_data (0x44);
	d395t9375v0_spi_write_cmd (0xE2);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_data (0x44);
	d395t9375v0_spi_write_data (0x44);
	d395t9375v0_spi_write_data (0xED);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0xEC);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_cmd (0xE3);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_cmd (0xE4);
	d395t9375v0_spi_write_data (0x44);
	d395t9375v0_spi_write_data (0x44);
	d395t9375v0_spi_write_cmd (0xE5);
	d395t9375v0_spi_write_data (0x0A);
	d395t9375v0_spi_write_data (0xE9);
	d395t9375v0_spi_write_data (0xD8);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_data (0x0C);
	d395t9375v0_spi_write_data (0xEB);
	d395t9375v0_spi_write_data (0xD8);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_data (0x0E);
	d395t9375v0_spi_write_data (0xED);
	d395t9375v0_spi_write_data (0xD8);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_data (0x10);
	d395t9375v0_spi_write_data (0xEF);
	d395t9375v0_spi_write_data (0xD8);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_cmd (0xE6);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_data (0x11);
	d395t9375v0_spi_write_cmd (0xE7);
	d395t9375v0_spi_write_data (0x44);
	d395t9375v0_spi_write_data (0x44);
	d395t9375v0_spi_write_cmd (0xE8);
	d395t9375v0_spi_write_data (0x09);
	d395t9375v0_spi_write_data (0xE8);
	d395t9375v0_spi_write_data (0xD8);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_data (0x0B);
	d395t9375v0_spi_write_data (0xEA);
	d395t9375v0_spi_write_data (0xD8);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_data (0x0D);
	d395t9375v0_spi_write_data (0xEC);
	d395t9375v0_spi_write_data (0xD8);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_data (0x0F);
	d395t9375v0_spi_write_data (0xEE);
	d395t9375v0_spi_write_data (0xD8);
	d395t9375v0_spi_write_data (0xA0);
	d395t9375v0_spi_write_cmd (0xEB);
	d395t9375v0_spi_write_data (0x02);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0xE4);
	d395t9375v0_spi_write_data (0xE4);
	d395t9375v0_spi_write_data (0x88);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x40);
	d395t9375v0_spi_write_cmd (0xEC);
	d395t9375v0_spi_write_data (0x3C);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_cmd (0xED);
	d395t9375v0_spi_write_data (0xAB);
	d395t9375v0_spi_write_data (0x89);
	d395t9375v0_spi_write_data (0x76);
	d395t9375v0_spi_write_data (0x54);
	d395t9375v0_spi_write_data (0x02);
	d395t9375v0_spi_write_data (0xFF);
	d395t9375v0_spi_write_data (0xFF);
	d395t9375v0_spi_write_data (0xFF);
	d395t9375v0_spi_write_data (0xFF);
	d395t9375v0_spi_write_data (0xFF);
	d395t9375v0_spi_write_data (0xFF);
	d395t9375v0_spi_write_data (0x20);
	d395t9375v0_spi_write_data (0x45);
	d395t9375v0_spi_write_data (0x67);
	d395t9375v0_spi_write_data (0x98);
	d395t9375v0_spi_write_data (0xBA);
	d395t9375v0_spi_write_cmd (0xFF);
	d395t9375v0_spi_write_data (0x77);
	d395t9375v0_spi_write_data (0x01);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	d395t9375v0_spi_write_data (0x00);
	/*
	d395t9375v0_spi_write_cmd (0x3A);
	d395t9375v0_spi_write_data (0x66);
	*/
	d395t9375v0_spi_write_cmd (0x11);
	sunxi_lcd_delay_ms(120);
	d395t9375v0_spi_write_cmd (0x29);
	sunxi_lcd_delay_ms(20);
}

static void LCD_panel_exit(u32 sel)
{
	return;
}

/* sel: 0:lcd0; 1:lcd1 */
static s32 LCD_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

struct __lcd_panel d395t9375v0_panel = {
	/* panel driver name, must mach the name of lcd_drv_name in sys_config.fex */
	.name = "d395t9375v0",
	.func = {
		.cfg_panel_info = LCD_cfg_panel_info,
		.cfg_open_flow = LCD_open_flow,
		.cfg_close_flow = LCD_close_flow,
		.lcd_user_defined_func = LCD_user_defined_func,
	},
};
