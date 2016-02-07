/* linux/arch/arm/mach-msm/board-vivo-panel.c
 *
 * Copyright (C) 2008 HTC Corporation.
 * Author: Jay Tu <jay_tu@htc.com>
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
#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/gpio.h>

#include <mach/msm_panel.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <mach/msm_fb-7x30.h>
#include <mach/msm_iomap.h>
#include <mach/vreg.h>

#include "../board-vivo.h"
#include "../devices.h"
#include "../proc_comm.h"
#include "../../../drivers/video/msm_7x30/mdp_hw.h"

#if 1
#define B(s...) printk(s)
#else
#define B(s...) do {} while(0)
#endif

extern unsigned long msm_fb_base;

#define DEFAULT_BRIGHTNESS 100
extern int panel_type;

#define VIVO_BR_DEF_USER_PWM		143
#define VIVO_BR_MIN_USER_PWM		30
#define VIVO_BR_MAX_USER_PWM		255
#define VIVO_BR_DEF_SONY_PANEL_PWM		135
#define VIVO_BR_MIN_SONY_PANEL_PWM		9
#define VIVO_BR_MAX_SONY_PANEL_PWM		255

#define LCM_CMD(_cmd, _delay, ...)                              \
{                                                               \
        .cmd = _cmd,                                            \
        .delay = _delay,                                        \
        .vals = (u8 []){__VA_ARGS__},                           \
        .len = sizeof((u8 []){__VA_ARGS__}) / sizeof(u8)        \
}
static struct clk *axi_clk;

static struct vreg *V_LCMIO_1V8, *V_LCM_2V85;
static struct cabc_t {
	struct led_classdev lcd_backlight;
	struct msm_mddi_client_data *client_data;
	struct mutex lock;
	unsigned long status;
} cabc;

enum {
	GATE_ON = 1 << 0,
	CABC_STATE,
};

static struct msm_mdp_platform_data mdp_pdata_sony = {
	.overrides = MSM_MDP4_MDDI_DMA_SWITCH,
#ifdef CONFIG_MDP4_HW_VSYNC
	.xres = 480,
	.yres = 800,
	.back_porch = 4,
	.front_porch = 2,
	.pulse_width = 4,
#endif
};

enum led_brightness vivo_brightness_value = DEFAULT_BRIGHTNESS;// multiple definition of `brightness_value' in board-glacier-panel.c

static int vivo_shrink_pwm(int brightness)
{
	int level;
	unsigned int min_pwm, def_pwm, max_pwm;
	
	min_pwm = VIVO_BR_MIN_SONY_PANEL_PWM;
	def_pwm = VIVO_BR_DEF_SONY_PANEL_PWM;
	max_pwm = VIVO_BR_MAX_SONY_PANEL_PWM;
	if (brightness <= VIVO_BR_DEF_USER_PWM) {
		if (brightness <= VIVO_BR_MIN_USER_PWM)
			level = min_pwm;
		else
			level = (def_pwm - min_pwm) *
				(brightness - VIVO_BR_MIN_USER_PWM) /
				(VIVO_BR_DEF_USER_PWM - VIVO_BR_MIN_USER_PWM) +
				min_pwm;
	} else
		level = (max_pwm - def_pwm) *
		(brightness - VIVO_BR_DEF_USER_PWM) /
		(VIVO_BR_MAX_USER_PWM - VIVO_BR_DEF_USER_PWM) +
		def_pwm;

	return level;
}

/* use one flag to have better backlight on/off performance */
static int vivo_set_dim = 1;

static void vivo_set_brightness(struct led_classdev *led_cdev,
				enum led_brightness val)
{
	struct msm_mddi_client_data *client = cabc.client_data;
	unsigned int shrink_br = val;

	if (test_bit(GATE_ON, &cabc.status) == 0)
		return;
	shrink_br = vivo_shrink_pwm(val);

	if(!client) {
		pr_info("null mddi client");
		return;
	}

	mutex_lock(&cabc.lock);
	if (vivo_set_dim == 1) {
		client->remote_write(client, 0x2C, 0x5300);
		/* we dont need set dim again */
		vivo_set_dim = 0;
	}
	client->remote_write(client, 0x00, 0x5500);
	client->remote_write(client, shrink_br, 0x5100);
	
	vivo_brightness_value = val;
	mutex_unlock(&cabc.lock);
}

static enum led_brightness
vivo_get_brightness(struct led_classdev *led_cdev)
{
	return vivo_brightness_value;
}

static void vivo_backlight_switch(int on)
{
	enum led_brightness val;

	if (on) {
		printk(KERN_DEBUG "turn on backlight\n");
		set_bit(GATE_ON, &cabc.status);
		val = cabc.lcd_backlight.brightness;

		/* LED core uses get_brightness for default value
		 * If the physical layer is not ready, we should
		 * not count on it */
		if (val == 0)
			val = DEFAULT_BRIGHTNESS;
		vivo_set_brightness(&cabc.lcd_backlight, val);
		/* set next backlight value with dim */
		vivo_set_dim = 1;
	} else {
		clear_bit(GATE_ON, &cabc.status);
		vivo_set_brightness(&cabc.lcd_backlight, 0);
	}
}

static int vivo_cabc_switch(int on)
{
	struct msm_mddi_client_data *client = cabc.client_data;

	if (test_bit(CABC_STATE, &cabc.status) == on)
               return 1;

	if (on) {
		printk(KERN_DEBUG "turn on CABC\n");
		set_bit(CABC_STATE, &cabc.status);
		mutex_lock(&cabc.lock);
		client->remote_write(client, 0x01, 0x5500);
		client->remote_write(client, 0x2C, 0x5300);
		mutex_unlock(&cabc.lock);
	} else {
		printk(KERN_DEBUG "turn off CABC\n");
		clear_bit(CABC_STATE, &cabc.status);
		mutex_lock(&cabc.lock);
		client->remote_write(client, 0x00, 0x5500);
		client->remote_write(client, 0x2C, 0x5300);
		mutex_unlock(&cabc.lock);
	}
	return 1;
}

static ssize_t
auto_backlight_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t
auto_backlight_store(struct device *dev, struct device_attribute *attr,
               const char *buf, size_t count);
#define CABC_ATTR(name) __ATTR(name, 0644, auto_backlight_show, auto_backlight_store)

static struct device_attribute auto_attr = CABC_ATTR(auto);
static ssize_t
auto_backlight_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0;

	i += scnprintf(buf + i, PAGE_SIZE - 1, "%d\n",
				test_bit(CABC_STATE, &cabc.status));
	return i;
}

static ssize_t
auto_backlight_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int rc;
	unsigned long res;

	rc = strict_strtoul(buf, 10, &res);
	if (rc) {
		printk(KERN_ERR "invalid parameter, %s %d\n", buf, rc);
		count = -EINVAL;
		goto err_out;
	}

	if (vivo_cabc_switch(!!res))
		count = -EIO;

err_out:
	return count;
}

static int vivo_backlight_probe(struct platform_device *pdev)
{
	int err = -EIO;

	B(KERN_DEBUG "%s(%d)\n", __func__, __LINE__);
	mutex_init(&cabc.lock);
	cabc.client_data = pdev->dev.platform_data;
	cabc.lcd_backlight.name = "lcd-backlight";
	cabc.lcd_backlight.brightness_set = vivo_set_brightness;
	cabc.lcd_backlight.brightness_get = vivo_get_brightness;
	err = led_classdev_register(&pdev->dev, &cabc.lcd_backlight);
	if (err)
		goto err_register_lcd_bl;

	err = device_create_file(cabc.lcd_backlight.dev, &auto_attr);
	if (err)
		goto err_out;

	return 0;

err_out:
	device_remove_file(&pdev->dev, &auto_attr);
err_register_lcd_bl:
	led_classdev_unregister(&cabc.lcd_backlight);
	return err;
}

/* ------------------------------------------------------------------- */

static struct resource resources_msm_fb[] = {
	{
		.flags = IORESOURCE_MEM,
	},
};

#define REG_WAIT (0xffff)

struct nov_regs {
	unsigned reg;
	unsigned val;
};

static struct nov_regs sony_init_seq[] = {
	{0x1100, 0x00},
	{REG_WAIT, 10},
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

static int
vivo_mddi_init(struct msm_mddi_bridge_platform_data *bridge_data,
		     struct msm_mddi_client_data *client_data)
{
	int i = 0, array_size = 0;
	unsigned reg, val;
	struct nov_regs *init_seq= NULL;
	B(KERN_DEBUG "%s\n", __func__);
	client_data->auto_hibernate(client_data, 0);

	init_seq = sony_init_seq;
	array_size = ARRAY_SIZE(sony_init_seq);
	
	for (i = 0; i < array_size; i++) {
		reg = cpu_to_le32(init_seq[i].reg);
		val = cpu_to_le32(init_seq[i].val);
		if (reg == REG_WAIT)
				msleep(val);
		else {
			client_data->remote_write(client_data, val, reg);
			if (reg == 0x1100)
				client_data->send_powerdown(client_data);
		}
	}
	client_data->auto_hibernate(client_data, 1);

	if(axi_clk)
		clk_set_rate(axi_clk, 0);

	return 0;
}

static int
vivo_mddi_uninit(struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	B(KERN_DEBUG "%s(%d)\n", __func__, __LINE__);
	return 0;
}

static int
vivo_panel_blank(struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	B(KERN_DEBUG "%s(%d)\n", __func__, __LINE__);
	client_data->auto_hibernate(client_data, 0);
	
	client_data->remote_write(client_data, 0x0, 0x5300);
	vivo_backlight_switch(LED_OFF);
	client_data->remote_write(client_data, 0, 0x2800);
	client_data->remote_write(client_data, 0, 0x1000);
	
	client_data->auto_hibernate(client_data, 1);
	return 0;
}

static int
vivo_panel_unblank(struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	B(KERN_DEBUG "%s(%d)\n", __func__, __LINE__);
	client_data->auto_hibernate(client_data, 0);
	
	client_data->remote_write(client_data, 0x24, 0x5300);
	client_data->remote_write(client_data, 0x0A, 0x22C0);
	msleep(30);
	vivo_backlight_switch(LED_FULL);
	
	client_data->auto_hibernate(client_data, 1);
	
	return 0;
}

static struct msm_mddi_bridge_platform_data novatec_client_data = {
	.init = vivo_mddi_init,
	.uninit = vivo_mddi_uninit,
	.blank = vivo_panel_blank,
	.unblank = vivo_panel_unblank,
	.fb_data = {
		.xres = 480,
		.yres = 800,
		.width = 52,
		.height = 86,
		.output_format = 0,
	},
	.panel_conf = {
		.caps = MSMFB_CAP_CABC,
		.vsync_gpio = 30,
	},
};

static void
mddi_power(struct msm_mddi_client_data *client_data, int on)
{
	int rc;
	unsigned config;
	B(KERN_DEBUG "%s(%d)\n", __func__, __LINE__);

	if (on) {
		if(axi_clk)
			clk_set_rate(axi_clk, 192000000);

		config = PCOM_GPIO_CFG(VIVO_MDDI_TE, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = PCOM_GPIO_CFG(VIVO_LCD_ID1, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = PCOM_GPIO_CFG(VIVO_LCD_ID0, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);

		vreg_enable(V_LCM_2V85);
		vreg_enable(V_LCMIO_1V8);

		gpio_set_value(VIVO_LCD_RSTz, 1);
		msleep(1);
		gpio_set_value(VIVO_LCD_RSTz, 0);
		msleep(1);
		gpio_set_value(VIVO_LCD_RSTz, 1);
		msleep(5);

	} else {

		msleep(10);
		gpio_set_value(VIVO_LCD_RSTz, 0);
		msleep(5);
		vreg_disable(V_LCMIO_1V8);
		vreg_disable(V_LCM_2V85);

		config = PCOM_GPIO_CFG(VIVO_MDDI_TE, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = PCOM_GPIO_CFG(VIVO_LCD_ID1, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = PCOM_GPIO_CFG(VIVO_LCD_ID0, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
	}
}

static void mddi_fixup(uint16_t *mfr_name, uint16_t *product_code)
{
	printk(KERN_INFO "mddi fixup\n");
	
	*mfr_name = 0xb9f6;
	*product_code = 0x5560;
}

static struct msm_mddi_platform_data mddi_pdata = {
	.fixup = mddi_fixup,
	.power_client = mddi_power,
	.fb_resource = resources_msm_fb,
	.num_clients = 2,
	.client_platform_data = {
		{
			.product_id = (0xb9f6 << 16 | 0x5560),
			.name = "mddi_c_b9f6_5560",
			.id = 0,
			.client_data = &novatec_client_data,
			.clk_rate = 0,
		},
	},
};

static struct platform_driver vivo_backlight_driver = {
	.probe = vivo_backlight_probe,
	.driver = {
		.name = "nov_cabc",
		.owner = THIS_MODULE,
	},
};

int __init vivo_init_panel(void)
{
	int rc;

	B(KERN_INFO "%s(%d): enter. panel_type 0x%08x\n", __func__, __LINE__, panel_type);
	
	msm_device_mdp.dev.platform_data = &mdp_pdata_sony;

	rc = platform_device_register(&msm_device_mdp);
	if (rc)
		return rc;

	mddi_pdata.clk_rate = 384000000;

	mddi_pdata.type = MSM_MDP_MDDI_TYPE_II;

	axi_clk = clk_get(NULL, "ebi1_mddi_clk");
	if (IS_ERR(axi_clk)) {
		pr_err("%s: failed to get axi clock\n", __func__);
		return PTR_ERR(axi_clk);
	}

	resources_msm_fb[0].start = msm_fb_base;
	resources_msm_fb[0].end = msm_fb_base + MSM_FB_SIZE - 1;

	msm_device_mddi0.dev.platform_data = &mddi_pdata;
	rc = platform_device_register(&msm_device_mddi0);
	if (rc)
		return rc;

	rc = platform_driver_register(&vivo_backlight_driver);
	if (rc)
		return rc;

	/* lcd panel power */
	/* 2.85V -- LDO20 */

	V_LCM_2V85 = vreg_get(NULL, "gp13");

	if (IS_ERR(V_LCM_2V85)) {
		pr_err("%s: LCM_2V85 get failed (%ld)\n",
			__func__, PTR_ERR(V_LCM_2V85));
		return -1;
	}
	V_LCMIO_1V8 = vreg_get(NULL, "lvsw0");

	if (IS_ERR(V_LCMIO_1V8)) {
		pr_err("%s: LCMIO_1V8 get failed (%ld)\n",
		       __func__, PTR_ERR(V_LCMIO_1V8));
		return -1;
	}

	return 0;
}
