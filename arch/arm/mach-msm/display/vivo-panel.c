/* linux/arch/arm/mach-msm/board-vivo-panel.c
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

#include "../board-vivo.h"
#include "../devices.h"
#include "../proc_comm.h"


int device_fb_detect_panel(const char *name)
{
	if (!strcmp(name, "mddi_vivo_wvga")) {
		return 0;
	}
	return -ENODEV;
}

static struct vreg *V_LCMIO_1V8, *V_LCMIO_2V8;

int panel_init_power(void) 
{
	/* lcd panel power */
	/* 2.85V -- LDO20 */

	V_LCMIO_2V8 = vreg_get(NULL, "gp13");
	if (IS_ERR(V_LCMIO_2V8))
		return PTR_ERR(V_LCMIO_2V8);

	V_LCMIO_1V8 = vreg_get(NULL, "lvsw0");
	if (IS_ERR(V_LCMIO_1V8))
		return PTR_ERR(V_LCMIO_1V8);

	return 0;
}

static struct msm_panel_common_pdata mddi_vivowvga_pdata;
static struct platform_device mddi_vivowvga_device = {
	.name   = "mddi_vivo_wvga",
	.id     = 0,
	.dev    = {
		.platform_data = &mddi_vivowvga_pdata,
	}
};

static int mddi_novatec_power(int on)
{

	int rc;
	unsigned config;

	if (on) {

		config = GPIO_CFG(VIVO_MDDI_TE, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = GPIO_CFG(VIVO_LCD_ID1, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = GPIO_CFG(VIVO_LCD_ID0, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		
		vreg_enable(V_LCMIO_2V8);
		msleep(5);
		vreg_enable(V_LCMIO_1V8);
		msleep(3);

		gpio_set_value(VIVO_LCD_RSTz, 1);
		msleep(1);
		gpio_set_value(VIVO_LCD_RSTz, 0);
		msleep(1);
		gpio_set_value(VIVO_LCD_RSTz, 1);
		msleep(15);
		
	} else {
	
		msleep(40);
		gpio_set_value(VIVO_LCD_RSTz, 0);
		msleep(5);
		vreg_disable(V_LCMIO_1V8);
		vreg_disable(V_LCMIO_2V8);

		config = GPIO_CFG(VIVO_MDDI_TE, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = GPIO_CFG(VIVO_LCD_ID1, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = GPIO_CFG(VIVO_LCD_ID0, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
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
	.mem_hid = BIT(ION_CP_WB_HEAP_ID),
};

void __init vivo_mdp_writeback(struct memtype_reserve* reserve_table)
{
	mdp_pdata.ov0_wb_size = MSM_FB_OVERLAY0_WRITEBACK_SIZE;
}

int __init vivo_init_panel(void)
{
	int ret;

	ret = panel_init_power();
	if (ret)
		return ret;

	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("pmdh", &mddi_pdata);

	ret = platform_device_register(&mddi_vivowvga_device);

	return 0;
}
