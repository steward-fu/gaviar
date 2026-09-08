/* drivers/video/sunxi/disp2/disp/lcd/tft08006.c
 *
 * Copyright (c) 2021 Allwinnertech Co., Ltd.
 *
 * tft08006 panel driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include "tft08006.h"

static void lcd_power_on(u32 sel);
static void lcd_power_off(u32 sel);
static void lcd_bl_open(u32 sel);
static void lcd_bl_close(u32 sel);

static void lcd_panel_init(u32 sel);
static void lcd_panel_exit(u32 sel);

#define panel_reset(sel, val) sunxi_lcd_gpio_set_value(sel, 0, val)

static void lcd_cfg_panel_info(struct panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
		{0, 0},
		{15, 15},
		{30, 30},
		{45, 45},
		{60, 60},
		{75, 75},
		{90, 90},
		{105, 105},
		{120, 120},
		{135, 135},
		{150, 150},
		{165, 165},
		{180, 180},
		{195, 195},
		{210, 210},
		{225, 225},
		{240, 240},
		{255, 255},
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

			value = lcd_gamma_tbl[i][1] +
				((lcd_gamma_tbl[i + 1][1] - lcd_gamma_tbl[i][1])
				 * j) / num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
				(value << 16)
				+ (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items - 1][1] << 16) +
				   (lcd_gamma_tbl[items - 1][1] << 8)
				   + lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));

}

static s32 lcd_open_flow(u32 sel)
{
	LCD_OPEN_FUNC(sel, lcd_power_on, 10);
	LCD_OPEN_FUNC(sel, lcd_panel_init, 10);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 50);
	LCD_OPEN_FUNC(sel, lcd_bl_open, 0);

	return 0;
}

static s32 lcd_close_flow(u32 sel)
{
	LCD_CLOSE_FUNC(sel, lcd_bl_close, 0);
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	LCD_CLOSE_FUNC(sel, lcd_panel_exit, 200);
	LCD_CLOSE_FUNC(sel, lcd_power_off, 500);

	return 0;
}

static void lcd_power_on(u32 sel)
{
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(100);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(20);
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(100);
	sunxi_lcd_pin_cfg(sel, 1);

}

static void lcd_power_off(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);
	sunxi_lcd_delay_ms(20);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(5);
}

static void lcd_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);
}

static void lcd_bl_close(u32 sel)
{
	sunxi_lcd_pwm_disable(sel);
}


static void lcd_panel_init(u32 sel)
{


#if 0
	sunxi_lcd_cpu_write_index(sel, 0xE9); 
	sunxi_lcd_cpu_write_data(sel, 0x20); 
	sunxi_lcd_cpu_write_index(sel, 0x11); //Exit Sleep 
	sunxi_lcd_delay_ms(100); 

	sunxi_lcd_cpu_write_index(sel, 0x2b); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x20); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0xaf);

	sunxi_lcd_cpu_write_index(sel, 0xD1); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x6B); 
	sunxi_lcd_cpu_write_data(sel, 0x19); 

	sunxi_lcd_cpu_write_index(sel, 0xD0); 
	sunxi_lcd_cpu_write_data(sel, 0x07); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x04); 

	sunxi_lcd_cpu_write_index(sel, 0xC1); 
	sunxi_lcd_cpu_write_data(sel, 0x10); 
	sunxi_lcd_cpu_write_data(sel, 0x10); 
	sunxi_lcd_cpu_write_data(sel, 0x02); 
	sunxi_lcd_cpu_write_data(sel, 0x02); 

	sunxi_lcd_cpu_write_index(sel, 0xC0); //Set Default Gamma 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x35); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x02); 

	sunxi_lcd_cpu_write_index(sel, 0xC5); //Set frame rate TE RATE
	sunxi_lcd_cpu_write_data(sel, 0x07); 

	sunxi_lcd_cpu_write_index(sel, 0xD2); //power setting 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x44);

	sunxi_lcd_cpu_write_index(sel, 0xC8); //Set Gamma 
	sunxi_lcd_cpu_write_data(sel, 0x04); 
	sunxi_lcd_cpu_write_data(sel, 0x67); 
	sunxi_lcd_cpu_write_data(sel, 0x35); 
	sunxi_lcd_cpu_write_data(sel, 0x04); 
	sunxi_lcd_cpu_write_data(sel, 0x08); 
	sunxi_lcd_cpu_write_data(sel, 0x06); 
	sunxi_lcd_cpu_write_data(sel, 0x24); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x37); 
	sunxi_lcd_cpu_write_data(sel, 0x40); 
	sunxi_lcd_cpu_write_data(sel, 0x03); 
	sunxi_lcd_cpu_write_data(sel, 0x10); 
	sunxi_lcd_cpu_write_data(sel, 0x08); 
	sunxi_lcd_cpu_write_data(sel, 0x80); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 

	sunxi_lcd_cpu_write_index(sel, 0XC9); //Set Gamma 
	sunxi_lcd_cpu_write_data(sel, 0x0D); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x0B); 
	sunxi_lcd_cpu_write_data(sel, 0x0B); 
	sunxi_lcd_cpu_write_data(sel, 0x0D); 
	sunxi_lcd_cpu_write_data(sel, 0x0D); 
	sunxi_lcd_cpu_write_data(sel, 0x0E); 
	sunxi_lcd_cpu_write_data(sel, 0x0E); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00);

	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01);

	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x04); 
	sunxi_lcd_cpu_write_data(sel, 0x0D); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x09); 
	sunxi_lcd_cpu_write_data(sel, 0x0B); 
	sunxi_lcd_cpu_write_data(sel, 0x0B); 
	sunxi_lcd_cpu_write_data(sel, 0x0D); 
	sunxi_lcd_cpu_write_data(sel, 0x0D); 
	sunxi_lcd_cpu_write_data(sel, 0x0E); 
	sunxi_lcd_cpu_write_data(sel, 0x0E); 
	sunxi_lcd_cpu_write_data(sel, 0x0E); 
	sunxi_lcd_cpu_write_data(sel, 0x0E); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x00); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01);

	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x01); 
	sunxi_lcd_cpu_write_data(sel, 0x04); 
	sunxi_lcd_cpu_write_data(sel, 0x04);

	//sunxi_lcd_cpu_write_index(sel, 0x3A);//SET RGB565(0X55)/RGB666(0X66)
	//sunxi_lcd_cpu_write_data(sel, 0x55);

	sunxi_lcd_cpu_write_index(sel, 0x35);//TE ON
	sunxi_lcd_cpu_write_data(sel, 0x00); 

	sunxi_lcd_cpu_write_index(sel, 0x36);// SET RGB MODE AND UV RL
	sunxi_lcd_cpu_write_data(sel, 0x88);


	sunxi_lcd_cpu_write_index(sel, 0xEA); //Enable 3 Gamma 
	sunxi_lcd_cpu_write_data(sel, 0x80); 


	sunxi_lcd_cpu_write_index(sel, 0x29); //display on
	sunxi_lcd_delay_ms(100); 
	sunxi_lcd_cpu_write_index(sel, 0x2c);
	sunxi_lcd_delay_ms(20); 

	printk("lovexulu test display init! %s %d ------------------------\n", __FUNCTION__, __LINE__);
#endif
	/*lcd_cs, active low */

/*----------------------------------------------------
	sunxi_lcd_cpu_write_index(sel, 0x2a);// column address set
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0xef);
	
	sunxi_lcd_cpu_write_index(sel, 0x2b);// row address set
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0xef); 


	sunxi_lcd_cpu_write_index(sel, 0xb2);//??
	sunxi_lcd_cpu_write_data(sel, 0x5c);
	sunxi_lcd_cpu_write_data(sel, 0x5c);
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0x33);
	sunxi_lcd_cpu_write_data(sel, 0x33); 

	sunxi_lcd_cpu_write_index(sel, 0xb7);
	sunxi_lcd_cpu_write_data(sel, 0x35);
---------------------------------------------------*/
	/*ST7789V Power setting */
#if 0
	sunxi_lcd_cpu_write_index(sel, 0xbb);
	sunxi_lcd_cpu_write_data(sel, 0x1e);
	sunxi_lcd_cpu_write_index(sel, 0xc0);
	sunxi_lcd_cpu_write_data(sel, 0x2c);
	sunxi_lcd_cpu_write_index(sel, 0xc2);
	sunxi_lcd_cpu_write_data(sel, 0x01);

	sunxi_lcd_cpu_write_index(sel, 0xc3);
	sunxi_lcd_cpu_write_data(sel, 0x0b);
	sunxi_lcd_cpu_write_index(sel, 0xc4);
	sunxi_lcd_cpu_write_data(sel, 0x20);

	//sunxi_lcd_cpu_write_index(sel, 0xc6);// frame rate control
	//sunxi_lcd_cpu_write_data(sel, 0x04);

	sunxi_lcd_cpu_write_index(sel, 0xd0);
	sunxi_lcd_cpu_write_data(sel, 0xa4);
	sunxi_lcd_cpu_write_data(sel, 0xa1);
	sunxi_lcd_cpu_write_index(sel, 0xd6);
	sunxi_lcd_cpu_write_data(sel, 0xa1);
	sunxi_lcd_cpu_write_index(sel, 0xbb);
	sunxi_lcd_cpu_write_data(sel, 0x1a);
#endif
	/*ST7789V gamma setting */

	sunxi_lcd_cpu_write_index(sel, 0xe0);
	sunxi_lcd_cpu_write_data(sel, 0xd0);
	sunxi_lcd_cpu_write_data(sel, 0x06);
	sunxi_lcd_cpu_write_data(sel, 0x0b);
	sunxi_lcd_cpu_write_data(sel, 0x07);
	sunxi_lcd_cpu_write_data(sel, 0x07);
	sunxi_lcd_cpu_write_data(sel, 0x24);
	sunxi_lcd_cpu_write_data(sel, 0x2e);
	sunxi_lcd_cpu_write_data(sel, 0x32);
	sunxi_lcd_cpu_write_data(sel, 0x46);
	sunxi_lcd_cpu_write_data(sel, 0x37);
	sunxi_lcd_cpu_write_data(sel, 0x13);
	sunxi_lcd_cpu_write_data(sel, 0x13);
	sunxi_lcd_cpu_write_data(sel, 0x2d);
	sunxi_lcd_cpu_write_data(sel, 0x33);

	sunxi_lcd_cpu_write_index(sel, 0xe1);
	sunxi_lcd_cpu_write_data(sel, 0xd0);
	sunxi_lcd_cpu_write_data(sel, 0x02);
	sunxi_lcd_cpu_write_data(sel, 0x06);
	sunxi_lcd_cpu_write_data(sel, 0x09);
	sunxi_lcd_cpu_write_data(sel, 0x08);
	sunxi_lcd_cpu_write_data(sel, 0x05);
	sunxi_lcd_cpu_write_data(sel, 0x29);
	sunxi_lcd_cpu_write_data(sel, 0x44);
	sunxi_lcd_cpu_write_data(sel, 0x42);
	sunxi_lcd_cpu_write_data(sel, 0x38);
	sunxi_lcd_cpu_write_data(sel, 0x14);
	sunxi_lcd_cpu_write_data(sel, 0x14);
	sunxi_lcd_cpu_write_data(sel, 0x2a);
	sunxi_lcd_cpu_write_data(sel, 0x30);

	sunxi_lcd_cpu_write_index(sel, 0x21);
	sunxi_lcd_cpu_write_index(sel, 0x11);//sleep out 
	sunxi_lcd_delay_ms(120);
	
	sunxi_lcd_cpu_write_index(sel, 0x36);//data access control
	sunxi_lcd_cpu_write_data(sel, 0xb0);

	sunxi_lcd_cpu_write_index(sel, 0x2a);// column address set
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0x01);
	sunxi_lcd_cpu_write_data(sel, 0x3f);
	
	sunxi_lcd_cpu_write_index(sel, 0x2b);// row address set
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0x00);
	sunxi_lcd_cpu_write_data(sel, 0xef); 

	sunxi_lcd_cpu_write_index(sel, 0x3a);// cpu mode
	sunxi_lcd_cpu_write_data(sel, 0x55);
	sunxi_lcd_cpu_write_index(sel, 0x29);//display on
	sunxi_lcd_cpu_write_index(sel, 0x2c);//memory write
	printk(KERN_ERR "st7789v3 init over!");
}

static void lcd_panel_exit(u32 sel)
{
	//sunxi_lcd_dsi_dcs_write_0para(sel, 0x10);
	sunxi_lcd_delay_ms(80);
	//sunxi_lcd_dsi_dcs_write_0para(sel, 0x28);
	sunxi_lcd_delay_ms(50);
}

/*sel: 0:lcd0; 1:lcd1*/
static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

struct __lcd_panel tft08006_panel = {
	/* panel driver name, must mach the name of
	 * lcd_drv_name in sys_config.fex
	 */
	.name = "tft08006",
	.func = {
		.cfg_panel_info = lcd_cfg_panel_info,
		.cfg_open_flow = lcd_open_flow,
		.cfg_close_flow = lcd_close_flow,
		.lcd_user_defined_func = lcd_user_defined_func,
	},
};
