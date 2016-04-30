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
#include "msm_fb.h" 
#include "mddihost.h" 
#include "mddihosti.h"

#define write_client_reg(val, reg) mddi_queue_register_write(reg, val, TRUE, 0);

static struct mddi_panel_platform_data *pdata;
static struct msm_fb_panel_data vivowvga_panel_data;


#define REG_WAIT (0xffff)

struct nov_regs {
    unsigned reg;
    unsigned val;
};

static struct nov_regs sony_init_seq[] = {
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

static int write_seq(struct nov_regs *cmd_table, unsigned array_size)
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

static int vivo_panel_init(void)
{
	write_seq(sony_init_seq, ARRAY_SIZE(sony_init_seq));

	return 0;
}

static int mddi_vivo_panel_on(struct platform_device *pdev)
{
	vivo_panel_init();

	write_client_reg(0x24, 0x5300);
	write_client_reg(0x0A, 0x22C0);
	msleep(30);

	return 0;
}

static int mddi_vivo_panel_off(struct platform_device *pdev)
{
	write_client_reg(0x0, 0x5300);
	write_client_reg(0, 0x2800);
	write_client_reg(0, 0x1000);

	return 0;
}

static void mddi_vivo_panel_set_backlight(struct msm_fb_data_type *mfd)
{
	printk(KERN_DEBUG "[BL] Setting bl to %d\n", mfd->bl_level);    

	write_client_reg(0x00, 0x5500)
	write_client_reg(mfd->bl_level, 0x5100);
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
	panel_data->panel_info.clk_min = 192000000;
	panel_data->panel_info.clk_max = 192000000;
	panel_data->panel_info.fb_num = 2;
	panel_data->panel_info.bl_max = 255;
	panel_data->panel_info.bl_min = 8;
	MSM_FB_SINGLE_MODE_PANEL(&panel_data->panel_info);

	panel_data->panel_info.lcd.rev = 2;
	panel_data->panel_info.lcd.vsync_enable = TRUE;
	panel_data->panel_info.lcd.refx100 = 6000;
	panel_data->panel_info.lcd.v_back_porch = 4;
	panel_data->panel_info.lcd.v_front_porch = 2;
	panel_data->panel_info.lcd.v_pulse_width = 4;
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
