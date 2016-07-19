/* 
 * drivers/video/msm/mddi_vivo_wvga.c
 *
 * Vivo MDDI Panel Driver for caf framebuffer
 *
 * Copyright (c) 2009 Google Inc.
 * Copyright (c) 2009 HTC.
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
 */

#include <linux/interrupt.h>
#include <linux/wakelock.h> 
#include <linux/leds.h> 
#include <linux/delay.h> 
#include <linux/init.h> 
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <mach/panel_id.h>
#include "msm_fb.h" 
#include "mddihost.h" 
#include "mddihosti.h"

#define write_client_reg(val, reg)	mddi_queue_register_write(reg, val, TRUE, 0);

extern int panel_type;

static struct mddi_panel_platform_data *pdata;
static struct msm_fb_panel_data vivowvga_panel_data;

#define REG_WAIT (0xffff)

#define LCM_CMD(_cmd, _delay, ...)                              \
{                                                               \
        .cmd = _cmd,                                            \
        .delay = _delay,                                        \
        .vals = (u32 []){__VA_ARGS__},                           \
        .len = sizeof((u32 []){__VA_ARGS__}) / sizeof(u32)        \
}

struct mddi_cmd {
	unsigned cmd;
	unsigned delay;
	u32 *vals;
	unsigned len;
};

struct mddi_regs {
    unsigned reg;
    unsigned val;
};

static struct mddi_cmd hitachi_renesas_cmd[] = {
	LCM_CMD(0x2A, 0, 0x00, 0x00, 0x01, 0xDF),
	LCM_CMD(0x2B, 0, 0x00, 0x00, 0x03, 0x1F),
	LCM_CMD(0x36, 0, 0x00, 0x00, 0x00, 0x00),
	LCM_CMD(0x3A, 0, 0x77, 0x77, 0x77, 0x77),//set_pixel_format 0x66 for 18bit/pixel, 0x77 for 24bit/pixel
	LCM_CMD(0xB0, 0, 0x04, 0x00, 0x00, 0x00),
	LCM_CMD(0x35, 0, 0x00, 0x00, 0x00, 0x00),//TE enable
	LCM_CMD(0xB0, 0, 0x03, 0x00, 0x00, 0x00),
};

static struct mddi_cmd hitachi_renesas_driving_cmd[] = {
	LCM_CMD(0xB0, 0, 0x04, 0x00, 0x00, 0x00),
	LCM_CMD(0xC1, 0, 0x43, 0x31, 0x00, 0x00),
	LCM_CMD(0xB0, 0, 0x03, 0x00, 0x00, 0x00),
};

static struct mddi_cmd hitachi_renesas_backlight_cmd[] = {
	LCM_CMD(0xB9, 0, 0x00, 0x00, 0x00, 0x00,
			 0xff, 0x00, 0x00, 0x00,
			 0x03, 0x00, 0x00, 0x00,
			 0x08, 0x00, 0x00, 0x00,),
};

static struct mddi_regs sony_init_seq[] = {
	{0x1100, 0x00},
	{REG_WAIT, 120},
	{0x0480, 0x63},
	{0x0580, 0x63},
	{0x0680, 0x63},
	{0x5E00, 0x06},
	{0x1DC0, 0x3F},
	{0x1EC0, 0x40},
	{0x3BC0, 0xF3},
	{0x3DC0, 0xEF},
	{0x3FC0, 0xEB},
	{0x40C0, 0xE7},
	{0x41C0, 0xE3},
	{0x42C0, 0xDF},
	{0x43C0, 0xDB},
	{0x44C0, 0xD7},
	{0x45C0, 0xD3},
	{0x46C0, 0xCF},
	{0x3500, 0x00},
	{0x4400, 0x02},
	{0x4401, 0x58},
	{0x2900, 0x00},
};

static void do_renesas_cmd(struct mddi_cmd *cmd_table, ssize_t size)
{
	struct mddi_cmd *pcmd = NULL;
	int ret = 0;
	
	for (pcmd = cmd_table; pcmd < cmd_table + size; pcmd++)
	{
		ret = mddi_host_register_multiwrite(pcmd->cmd, pcmd->vals, pcmd->len, TRUE, 0, MDDI_HOST_PRIM);
		if (ret != 0)
			printk(KERN_ERR "%s: failed multiwrite (%d)\n", __func__, ret);
		
		if (pcmd->delay)
			msleep(pcmd->delay);
	}
}

static int write_seq(struct mddi_regs *cmd_table, unsigned array_size)
{
	int i;

	for (i = 0; i < array_size; i++)
	{
		if (cmd_table[i].reg == REG_WAIT)
		{
			msleep(cmd_table[i].val);
			continue;
		}
		write_client_reg(cmd_table[i].val, cmd_table[i].reg);
	}
	
	return 0;
}

static int mddi_vivo_panel_on(struct platform_device *pdev)
{
	switch (panel_type) {
		case PANEL_ID_VIVO_HITACHI:
			do_renesas_cmd(hitachi_renesas_cmd, ARRAY_SIZE(hitachi_renesas_cmd));
			write_client_reg(0x0, 0x11);
			msleep(125);
			do_renesas_cmd(hitachi_renesas_driving_cmd, ARRAY_SIZE(hitachi_renesas_driving_cmd));
			write_client_reg(0x0, 0x29);
			break;
		default:
			write_seq(sony_init_seq, ARRAY_SIZE(sony_init_seq));
			write_client_reg(0x24, 0x5300);
			write_client_reg(0x0A, 0x22C0);
			msleep(30);
			break;
	}

	return 0;
}

static int mddi_vivo_panel_off(struct platform_device *pdev)
{
	switch (panel_type) {
		case PANEL_ID_VIVO_HITACHI:
			write_client_reg(0x0, 0x28);
			write_client_reg(0x0, 0xB8);
			write_client_reg(0x0, 0x10);
			msleep(72);
			break;
		default:
			write_client_reg(0x0, 0x5300);
			write_client_reg(0, 0x2800);
			write_client_reg(0, 0x1000);
			break;
	}

	return 0;
}

static void mddi_vivo_panel_set_backlight(struct msm_fb_data_type *mfd)
{
	struct mddi_cmd *pcmd = hitachi_renesas_backlight_cmd;
	
	switch (panel_type) {
		case PANEL_ID_VIVO_HITACHI:
			pcmd->vals[4] = mfd->bl_level;
			write_client_reg(0x04, 0xB0);
			do_renesas_cmd(pcmd, ARRAY_SIZE(hitachi_renesas_backlight_cmd));
			write_client_reg(0x03, 0xB0);
			break;
		default:
			write_client_reg(mfd->bl_level, 0x5100);
			break;
	}
}

static int vivowvga_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
    .probe = vivowvga_probe,
    .driver = {
        .name = "mddi_vivo_wvga"
    },
};

static struct msm_fb_panel_data vivowvga_panel_data = {
	.on = mddi_vivo_panel_on,
	.off = mddi_vivo_panel_off,
	.set_backlight = mddi_vivo_panel_set_backlight,
};

static struct platform_device this_device = {
	.name = "mddi_vivo_wvga",
	.id = 1,
	.dev = {
		.platform_data = &vivowvga_panel_data,
	}
};

static int __init vivowvga_init(void)
{
    int ret;
	struct msm_fb_panel_data *panel_data = &vivowvga_panel_data;

	printk(KERN_DEBUG "%s\n", __func__);

#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT
    if (msm_fb_detect_client("mddi_vivo_wvga"))
        return -ENODEV;
#endif

	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	panel_data->panel_info.xres = 480;
	panel_data->panel_info.yres = 800;
	panel_data->panel_info.pdest = DISPLAY_1;
	panel_data->panel_info.type = MDDI_PANEL;
	panel_data->panel_info.mddi.vdopkt = MDDI_DEFAULT_PRIM_PIX_ATTR;
	panel_data->panel_info.wait_cycle = 0;
	panel_data->panel_info.bpp = 24;
	panel_data->panel_info.clk_rate = 192000000;
	panel_data->panel_info.clk_min = 190000000;
	panel_data->panel_info.clk_max = 200000000;
	panel_data->panel_info.fb_num = 2;
	panel_data->panel_info.bl_max = 255;
	panel_data->panel_info.bl_min = 1;
	MSM_FB_SINGLE_MODE_PANEL(&panel_data->panel_info);

	panel_data->panel_info.lcd.rev = 2;
	panel_data->panel_info.lcd.vsync_enable = TRUE;
	panel_data->panel_info.lcd.refx100 = 6000;
	if (panel_type == PANEL_ID_VIVO_HITACHI) {
		panel_data->panel_info.lcd.v_back_porch = 2;
		panel_data->panel_info.lcd.v_front_porch = 42;
		panel_data->panel_info.lcd.v_pulse_width = 2;
	} else {
		panel_data->panel_info.lcd.v_back_porch = 4;
		panel_data->panel_info.lcd.v_front_porch = 2;
		panel_data->panel_info.lcd.v_pulse_width = 4;
	}
	panel_data->panel_info.lcd.hw_vsync_mode = TRUE;
	panel_data->panel_info.lcd.vsync_notifier_period = 0;

	ret = platform_device_register(&this_device);
	if (ret) {
		printk(KERN_ERR "%s not able to register the device\n",
			__func__);
		platform_driver_unregister(&this_driver);
	}

	return ret;
}

device_initcall(vivowvga_init);
