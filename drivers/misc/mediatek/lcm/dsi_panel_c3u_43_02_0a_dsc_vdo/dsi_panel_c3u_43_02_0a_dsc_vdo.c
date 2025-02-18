// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

#define FRAME_WIDTH			(720)
#define FRAME_HEIGHT			(1600)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH		(70310)
#define LCM_PHYSICAL_HEIGHT		(156240)
#define LCM_DENSITY			(260)

#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE		0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF

extern bool nvt_gesture_flag;
int32_t panel_rst_gpio;
int32_t panel_bias_enn_gpio;
int32_t panel_bias_enp_gpio;

bool lcd_reset_keep_high = false;
void set_lcd_reset_gpio_keep_high(bool en)
{
	lcd_reset_keep_high = en;
}
EXPORT_SYMBOL(set_lcd_reset_gpio_keep_high);

int panel_gpio_config(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;
	panel_rst_gpio = of_get_named_gpio(np, "panel,gpio_lcd_rst", 0);
	if (gpio_is_valid(panel_rst_gpio)) {
		ret = gpio_request(panel_rst_gpio, "panel-tp-rst");
		if (ret) {
			pr_err("Failed to request panel-tp-rst GPIO\n");
			goto err_request_reset_gpio;
		}
	}
	panel_bias_enn_gpio = of_get_named_gpio(np, "panel,gpio_lcd_bias_enn", 0);
	if (gpio_is_valid(panel_bias_enn_gpio)) {
		ret = gpio_request(panel_bias_enn_gpio, "panel-bias-enn");
		if (ret) {
			pr_err("Failed to request panel-bias-enn GPIO\n");
			goto err_request_enn_gpio;
		}
	}
	panel_bias_enp_gpio = of_get_named_gpio(np, "panel,gpio_lcd_bias_enp", 0);
	if (gpio_is_valid(panel_bias_enp_gpio)) {
		ret = gpio_request(panel_bias_enp_gpio, "panel-bias-enp");
		if (ret) {
			pr_err("Failed to request panel-bias-enp GPIO\n");
			goto err_request_enp_gpio;
		}
	}
err_request_enp_gpio:
	gpio_free(panel_bias_enn_gpio);
err_request_enn_gpio:
	gpio_free(panel_rst_gpio);
err_request_reset_gpio:

	return 0;
}

static int lcm_driver_probe(struct device *dev, void const *data)
{
	int ret = 0;
	ret = panel_gpio_config(dev);
	if (ret) {
		pr_err("panel parse dt error\n");
		return -1;
	}

	return 0;
}

static const struct of_device_id lcm_platform_of_match[] = {
	{
		.compatible = "mediatek,panel",
		.data = 0,
	}, {
		/* sentinel */
	}
};

MODULE_DEVICE_TABLE(of, platform_of_match);

static int lcm_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;

	id = of_match_node(lcm_platform_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	return lcm_driver_probe(&pdev->dev, id->data);
}

static struct platform_driver lcm_driver = {
	.probe = lcm_platform_probe,
	.driver = {
		.name = "panel",
		.owner = THIS_MODULE,
		.of_match_table = lcm_platform_of_match,
	},
};

static int __init lcm_drv_init(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
	if (platform_driver_register(&lcm_driver)) {
		pr_notice("LCM: failed to register disp driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcm_drv_exit(void)
{
	platform_driver_unregister(&lcm_driver);
	pr_notice("LCM: Unregister lcm driver done\n");
}

late_initcall(lcm_drv_init);
module_exit(lcm_drv_exit);
MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
};
/*
static struct LCM_setting_table lcm_suspend_setting1[] = {
    {0x51, 2, {0x00, 0x00} },//CABC 8bit
    {0x53, 1, {0x00} },//CABC 8bit
};
*/
static struct LCM_setting_table lcm_suspend_no_off_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF,0x01,{0x2A}},
	{0xFB,0x01,{0x01}},
	{0xAE,0x01,{0xB2}},
	{0xB0,0x01,{0x86}},

	{0xFF,0x01,{0x23}},
	{REGFLAG_DELAY,1,{}},
	{0xFB,0x01,{0x01}},
	//Resolution Related
	{0x10,0x01,{0xFF}},
	{0x11,0x01,{0x00}},
	{0x12,0x01,{0xB4}},
	{0x15,0x01,{0xE9}},
	{0x16,0x01,{0x14}},

	{0x29,0x01,{0x10}},
	{0x2A,0x01,{0x30}},
	{0x2B,0x01,{0x20}},

	//UI Mode
	{0x30,0x01,{0xFF}},
	{0x31,0x01,{0xFE}},
	{0x32,0x01,{0xFD}},
	{0x33,0x01,{0xFC}},
	{0x34,0x01,{0xFB}},
	{0x35,0x01,{0xFA}},
	{0x36,0x01,{0xF8}},
	{0x37,0x01,{0xF6}},
	{0x38,0x01,{0xF4}},
	{0x39,0x01,{0xF2}},
	{0x3A,0x01,{0xF0}},
	{0x3B,0x01,{0xEE}},
	{0x3D,0x01,{0xEC}},
	{0x3F,0x01,{0xEA}},
	{0x40,0x01,{0xE8}},
	{0x41,0x01,{0xE6}},
	//Still Mode
	{0x45,0x01,{0xFF}},
	{0x46,0x01,{0xFD}},
	{0x47,0x01,{0xFB}},
	{0x48,0x01,{0xF7}},
	{0x49,0x01,{0xF3}},
	{0x4A,0x01,{0xEB}},
	{0x4B,0x01,{0xDE}},
	{0x4C,0x01,{0xB5}},
	{0x4D,0x01,{0xAE}},
	{0x4E,0x01,{0xA2}},
	{0x4F,0x01,{0x9B}},
	{0x50,0x01,{0x95}},
	{0x51,0x01,{0x90}},
	{0x52,0x01,{0x86}},
	{0x53,0x01,{0x75}},
	{0x54,0x01,{0x65}},
	//MOV Mode
	{0x58,0x01,{0xFF}},
	{0x59,0x01,{0xFD}},
	{0x5A,0x01,{0xFC}},
	{0x5B,0x01,{0xF9}},
	{0x5C,0x01,{0xF6}},
	{0x5D,0x01,{0xF2}},
	{0x5E,0x01,{0xEE}},
	{0x5F,0x01,{0xD9}},
	{0x60,0x01,{0xD2}},
	{0x61,0x01,{0xCE}},
	{0x62,0x01,{0xCB}},
	{0x63,0x01,{0xC7}},
	{0x64,0x01,{0xC3}},
	{0x65,0x01,{0xBF}},
	{0x66,0x01,{0xBB}},
	{0x67,0x01,{0xB3}},

	{0x00,0x01,{0x68}},//11bit
	{0x07,0x01,{0x00}},
	{0x08,0x01,{0x01}},
	{0x09,0x01,{0x80}},

        {0xFF,0x01,{0x27}},
	{0xFB,0x01,{0x01}},
	{0xEF,0x01,{0x00}},
	{0xF5,0x01,{0x77}},

	{0xFF,0x01,{0x10}},
	{REGFLAG_DELAY,1,{}},
	{0xFB,0x01,{0x01}},

//	{0x51,0x01,{0xFF}},
	{0x53,0x01,{0x2C}},
	{0x55,0x01,{0x00}},
	{0x68,0x02,{0x02,0x01}},
	//Display_on
	{0x11,0,{}},
	{REGFLAG_DELAY,110,{}},
	//Sleep_out
	{0x29,0,{}},
	{REGFLAG_DELAY,20,{}},
};

static struct LCM_setting_table bl_level[] = {
	{0x51,2,{0x07,0xFF}},
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(void *cmdq, struct LCM_setting_table *table,
		       unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;
		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
					 table[i].para_list, force_update);
			break;
		}
	}
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
static void lcm_dfps_int(struct LCM_DSI_PARAMS *dsi)
{
	struct dfps_info *dfps_params = dsi->dfps_params;

	dsi->dfps_enable = 1;
	dsi->dfps_default_fps = 6000;/*real fps * 100, to support float*/
	dsi->dfps_def_vact_tim_fps = 9000;/*real vact timing fps * 100*/
	/* traversing array must less than DFPS_LEVELS */
	/* DPFS_LEVEL0 */
	dfps_params[0].level = DFPS_LEVEL0;
	dfps_params[0].fps = 6000;/*real fps * 100, to support float*/
	dfps_params[0].vact_timing_fps = 9000;/*real vact timing fps * 100*/

	dfps_params[0].vertical_frontporch = 1228;


	/* DPFS_LEVEL1 */
	dfps_params[1].level = DFPS_LEVEL1;
	dfps_params[1].fps = 9000;/*real fps * 100, to support float*/
	dfps_params[1].vact_timing_fps = 9000;/*real vact timing fps * 100*/

	dfps_params[1].vertical_frontporch = 281;

	dsi->dfps_num = 2;
}
#endif

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH / 1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT / 1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;

	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
	pr_info("%s: lcm_dsi_mode %d\n", __func__, lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 10;
	params->dsi.vertical_frontporch = 282;
//	params->dsi.vertical_frontporch_for_low_power = 750;	//OTM no data
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 90;
	params->dsi.horizontal_frontporch = 48;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
  	params->dsi.ssc_range = 1 ;
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 482;
	params->dsi.PLL_CK_CMD = 482;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/****DynFPS start****/
	lcm_dfps_int(&(params->dsi));
	/****DynFPS end****/
#endif
}

/*static int lcm_bias_regulator_init(void)
{
	return 0;
}*/

static int lcm_bias_enable(void)
{
	gpio_set_value(panel_bias_enp_gpio, 1);
	MDELAY(3);

	gpio_set_value(panel_bias_enn_gpio, 1);
	MDELAY(5);
	return 0;
}

static int lcm_bias_disable(void)
{
	gpio_set_value(panel_bias_enn_gpio, 0);
	MDELAY(2);

	gpio_set_value(panel_bias_enp_gpio, 0);
	MDELAY(5);
	return 0;
}

/* turn on gate ic & control voltage to 5.5V */

/* turn on gate ic & control voltage to 5.5V */
static void lcm_init_power(void)
{
	lcm_bias_enable();
}

static void lcm_suspend_power(void)
{
	if (lcd_reset_keep_high || nvt_gesture_flag) {
		pr_info("[LCM]%s:bias_keep_on\n",__func__);
		return;
	}
	lcm_bias_disable();
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
	lcm_init_power();
}

static void lcm_init(void)
{
	gpio_set_value(panel_rst_gpio, 0);
	MDELAY(5);
	gpio_set_value(panel_rst_gpio, 1);
	MDELAY(5);
	gpio_set_value(panel_rst_gpio, 0);
	MDELAY(10);
	gpio_set_value(panel_rst_gpio, 1);
	MDELAY(15);

	push_table(NULL,
		init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);

	pr_info("%s:nt36525b_hd-lcm mode=vdo mode:%d\n", __func__, lcm_dsi_mode);
}

static void lcm_suspend(void)
{
	if (lcd_reset_keep_high) {
		push_table(NULL, lcm_suspend_no_off_setting,
		ARRAY_SIZE(lcm_suspend_no_off_setting), 1);
		pr_info("%s,nt36528 panel no off end!\n", __func__);
	} else {
	    push_table(NULL, lcm_suspend_setting,
		ARRAY_SIZE(lcm_suspend_setting), 1);
	    pr_info("%s,nt36525b_hd panel end!\n", __func__);
	}
}

static void lcm_resume(void)
{
	pr_info("%s,nt36525b_hd panel start!\n", __func__);
	lcm_init();
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int id[3] = {0x40, 0, 0};
	unsigned int data_array[3];
	unsigned char read_buf[3];

	data_array[0] = 0x00033700; /* set max return size = 3 */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x04, read_buf, 3); /* read lcm id */

	LCM_LOGI("ATA read = 0x%x, 0x%x, 0x%x\n",
		 read_buf[0], read_buf[1], read_buf[2]);

	if ((read_buf[0] == id[0]) &&
	    (read_buf[1] == id[1]) &&
	    (read_buf[2] == id[2]))
		ret = 1;
	else
		ret = 0;

	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	pr_info("%s,nt36525b_hd backlight: level = %d\n", __func__, level);
	if((0 != level) && (level <= 8))
		level = 8;
	level = level*77/100;
	bl_level[0].para_list[0] = level >> 8;
	bl_level[0].para_list[1] = level & 0xFF;

	push_table(handle, bl_level, ARRAY_SIZE(bl_level), 1);

}

static void lcm_update(unsigned int x,
	unsigned int y, unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

#ifdef LCM_SET_DISPLAY_ON_DELAY
	lcm_set_display_on();
#endif

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}
static unsigned int lcm_compare_id(void)
{
	return 1;
}

struct LCM_DRIVER dsi_panel_c3u_43_02_0a_dsc_vdo_lcm_drv = {
	.name = "dsi_panel_c3u_43_02_0a_dsc_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
};
