/* linux/arch/arm/mach-msm/board-primou-panel.c
 *
 * Copyright (c) 2010 HTC.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/gpio.h>

#include <asm/io.h>
#include <asm/mach-types.h>
#include <mach/msm_iomap.h>
#include <mach/vreg.h>
#include <linux/msm_ion.h>
#include <mach/panel_id.h>

#include "../board-primou.h"
#include "../devices.h"
#include "../proc_comm.h"


int device_fb_detect_panel(const char *name)
{
	if (!strcmp(name, "mddi_primou_wvga")) {
		return 0;
	}
	return -ENODEV;
}

static struct vreg *V_LCMIO_1V8, *V_LCMIO_2V8;

int panel_init_power(void) 
{
	/* lcd panel power */
	/* 2.85V -- LDO20 */

	V_LCMIO_1V8 = vreg_get(NULL, "gp13");
	if (IS_ERR(V_LCMIO_1V8))
		return PTR_ERR(V_LCMIO_1V8);

	V_LCMIO_2V8 = vreg_get(0, "wlan2");
	if (IS_ERR(V_LCMIO_2V8))
		return PTR_ERR(V_LCMIO_2V8);

    return 0;
}

static struct msm_panel_common_pdata mddi_primouwvga_pdata;
static struct platform_device mddi_primouwvga_device = {
	.name   = "mddi_primou_wvga",
	.id     = 0,
	.dev    = {
		.platform_data = &mddi_primouwvga_pdata,
	}
};

static int mddi_novatec_power(int on)
{

	int rc;
	unsigned config;

	if (on) {

		config = GPIO_CFG(PRIMOU_MDDI_TE, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = GPIO_CFG(PRIMOU_LCD_ID0, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);

		if (panel_type == PANEL_ID_PRIMO_SONY) {
			config = PCOM_GPIO_CFG(PRIMOU_LCD_ID1, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);
			rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
			vreg_enable(V_LCMIO_1V8);
			msleep(3);
			vreg_enable(V_LCMIO_2V8);
			msleep(5);

			gpio_set_value(PRIMOU_LCD_RSTz, 1);
			msleep(1);
			gpio_set_value(PRIMOU_LCD_RSTz, 0);
			msleep(1);
			gpio_set_value(PRIMOU_LCD_RSTz, 1);
			msleep(15);
		} else {
			config = GPIO_CFG(PRIMOU_LCD_ID1, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA);
			rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
			vreg_enable(V_LCMIO_1V8);
			msleep(3);
			vreg_enable(V_LCMIO_2V8);
			msleep(5);

			gpio_set_value(PRIMOU_LCD_RSTz, 1);
			msleep(1);
			gpio_set_value(PRIMOU_LCD_RSTz, 0);
			msleep(1);
			gpio_set_value(PRIMOU_LCD_RSTz, 1);
			msleep(20);
		}
	} else {
		if (panel_type == PANEL_ID_PRIMO_SONY) {
			msleep(40);
			gpio_set_value(PRIMOU_LCD_RSTz, 0);
			msleep(5);
			vreg_disable(V_LCMIO_1V8);
			vreg_disable(V_LCMIO_2V8);
		} else {
			msleep(20);
			gpio_set_value(PRIMOU_LCD_RSTz, 0);
			vreg_disable(V_LCMIO_2V8);
			vreg_disable(V_LCMIO_1V8);
		}

		config = GPIO_CFG(PRIMOU_MDDI_TE, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = GPIO_CFG(PRIMOU_LCD_ID1, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = GPIO_CFG(PRIMOU_LCD_ID0, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
	}
	return 0;
}

static int msm_fb_mddi_sel_clk(u32 *clk_rate)
{
    *clk_rate *= 2;
	return 0;
}

static struct mddi_platform_data mddi_pdata = {
	.mddi_power_save = mddi_novatec_power,
	.mddi_sel_clk = msm_fb_mddi_sel_clk,
};

static struct msm_panel_common_pdata mdp_pdata = {
	.hw_revision_addr = 0xac001270,
	.gpio = 30,
	.mdp_max_clk = 192000000,
	.mdp_rev = MDP_REV_40,
};

int __init primou_init_panel(void)
{
	int ret;

	switch (panel_type) {
	case PANEL_ID_PRIMO_SONY:
		printk("%s: Using sony panel.\n", __func__);
		break;
	case PANEL_ID_PRIMO_LG:
		printk("%s: Using lg panel.\n", __func__);
		break;
	default:
		printk("%s: Using some other panel.\n", __func__);
		break;
	}

	ret = panel_init_power();
	if (ret)
		return ret;

	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("pmdh", &mddi_pdata);

	ret = platform_device_register(&mddi_primouwvga_device);

	return 0;
}
