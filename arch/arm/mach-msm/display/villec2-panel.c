/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#if defined(CONFIG_ARCH_MSM8X60) && !defined(CONFIG_FB_MSM8960)
	#include "../../../../drivers/video/msm_8x60/msm_fb.h"
	#include "../../../../drivers/video/msm_8x60/mipi_dsi.h"
	#include "../../../../drivers/video/msm_8x60/mdp4.h"
#elif defined(CONFIG_ARCH_MSM8960) || defined(CONFIG_FB_MSM8960)
	#include "../../../../drivers/video/msm/msm_fb.h"
	#include "../../../../drivers/video/msm/mipi_dsi.h"
	#include "../../../../drivers/video/msm/mdp4.h"
#else
	#warning "Unsupported ARCH CONFIG"
#endif

#include <mach/gpio.h>
#include <mach/panel_id.h>
#include <mach/msm_bus_board.h>
#include <linux/mfd/pmic8058.h>
#include <linux/leds-pm8058.h>
#include <linux/pwm.h>
#include <linux/pmic8058-pwm.h>
#include <mach/debug_display.h>

#include "../devices.h"
#include "../board-villec2.h"
/* =============================================================================
 * LOCAL FUNCTION DEFINITIONS
 * =============================================================================
 */
static int mipi_dsi_panel_power(const int on);
static int __devinit villec2_lcd_probe(struct platform_device *pdev);
static void villec2_lcd_shutdown(struct platform_device *pdev);
static int villec2_lcd_on(struct platform_device *pdev);
static int villec2_lcd_off(struct platform_device *pdev);
static void villec2_set_backlight(struct msm_fb_data_type *mfd);
static void villec2_display_on(struct msm_fb_data_type *mfd);
static int mipi_villec2_device_register(const char* dev_name, struct msm_panel_info *pinfo, u32 channel, u32 panel);
#if defined (CONFIG_MSM_AUTOBL_ENABLE)
static int villec2_samsung_acl_enable(int on, struct msm_fb_data_type *mfd);
#endif

/* =============================================================================
 * COMMON DATA
 * =============================================================================
 */
static struct dsi_buf panel_tx_buf;
static struct dsi_buf panel_rx_buf;
static struct msm_panel_info pinfo;
static int cur_bl_level = 0;
static int mipi_lcd_on = 0;

/* static char sw_reset[2] = {0x01, 0x00};*//* DTYPE_DCS_WRITE */
static char enter_sleep[2] = {0x10, 0x00}; /* DTYPE_DCS_WRITE */
static char exit_sleep[2] = {0x11, 0x00}; /* DTYPE_DCS_WRITE */
static char display_off[2] = {0x28, 0x00}; /* DTYPE_DCS_WRITE */
static char display_on[2] = {0x29, 0x00}; /* DTYPE_DCS_WRITE */
static char enable_te[2] = {0x35, 0x00}; /* DTYPE_DCS_WRITE1 */

static struct platform_driver this_driver = {
	.probe  = villec2_lcd_probe,
	.shutdown = villec2_lcd_shutdown,
};

struct msm_fb_panel_data villec2_panel_data = {
	.on			= villec2_lcd_on,
	.off		= villec2_lcd_off,
	.set_backlight = villec2_set_backlight,
	.display_on = villec2_display_on,
#if defined (CONFIG_MSM_AUTOBL_ENABLE)
	.autobl_enable = villec2_samsung_acl_enable,
#endif
};

static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.vsync_gpio = 28,
	.dsi_power_save = mipi_dsi_panel_power,
};

/* =============================================================================
 * SAMSUNG PANEL DATA
 * =============================================================================
 */
static char samsung_panel_width[] = {0x2A, 0x00, 0x1E, 0x02, 0x39}; /* DTYPE_DCS_LWRITE */
static char samsung_panel_height[] = {0x2B, 0x00, 0x00, 0x03, 0xBF}; /* DTYPE_DCS_LWRITE */
static char samsung_panel_vinit[] = {0xD1, 0x8A}; /* DTYPE_DCS_WRITE1 */

static char samsung_e0[] = {0xF0, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
static char samsung_e1[] = {0xF1, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
static char samsung_e22[] = {0xFC, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
static char samsung_g0_c2[] = {
        0xFA, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; /* DTYPE_DCS_LWRITE */
static char samsung_g1[] = {0xFA, 0x03}; /* DTYPE_DCS_WRITE1 */
static char samsung_p0[] = {
	0xF8, 0x27, 0x27, 0x08,
	0x08, 0x4E, 0xAA, 0x5E,
	0x8A, 0x10, 0x3F, 0x10, 0x10, 0x00}; /* DTYPE_DCS_LWRITE */
static char samsung_e2[] = {0xF6, 0x00, 0x84, 0x09}; /* DTYPE_DCS_LWRITE */
static char samsung_e3[] = {0xB0, 0x09}; /* DTYPE_DCS_WRITE1 */
static char samsung_e4[] = {0xD5, 0x64}; /* DTYPE_DCS_WRITE1 */
static char samsung_e5[] = {0xB0, 0x0B}; /* DTYPE_DCS_WRITE1 */
static char samsung_e6[] = {0xD5, 0xA4, 0x7E, 0x20}; /* DTYPE_DCS_LWRITE */
static char samsung_e7[] = {0xF7, 0x03}; /* DTYPE_DCS_WRITE1 */
static char samsung_e8[] = {0xB0, 0x02}; /* DTYPE_DCS_WRITE1 */
static char samsung_e9[] = {0xB3, 0xC3}; /* DTYPE_DCS_WRITE1 */
static char samsung_e10[] = {0xB0, 0x08}; /* DTYPE_DCS_WRITE1 */
static char samsung_e11[] = {0xFD, 0xF8}; /* DTYPE_DCS_WRITE1 */
//static char samsung_e12[] = {0xB0, 0x04}; /* DTYPE_DCS_WRITE1 */
//static char samsung_e13[] = {0xF2, 0x4D}; /* DTYPE_DCS_WRITE1 */
static char samsung_te0[] = {0xB0, 0x01}; /* DTYPE_DCS_WRITE1 */
static char samsung_te1[] = {0xF2, 0x06, 0xDF, 0xFF, 0x4D}; /* DTYPE_DCS_LWRITE */
static char samsung_e14[] = {0xB0, 0x05}; /* DTYPE_DCS_WRITE1 */
static char samsung_e15[] = {0xFD, 0x1F}; /* DTYPE_DCS_WRITE1 */
static char samsung_e16[] = {0xB1, 0x01, 0x00, 0x16}; /* DTYPE_DCS_LWRITE */
static char samsung_e17_C2[] = {0xB2, 0x15, 0x15, 0x15, 0x15}; /* DTYPE_DCS_LWRITE */

static struct dsi_cmd_desc samsung_cmd_on_cmds_c2[] = {
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_e0), samsung_e0},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_e1), samsung_e1},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_e22), samsung_e22},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_g0_c2), samsung_g0_c2},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_g1), samsung_g1},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_p0), samsung_p0},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_e2), samsung_e2},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e3), samsung_e3},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e4), samsung_e4},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e5), samsung_e5},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_e6), samsung_e6},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e7), samsung_e7},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e8), samsung_e8},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e9), samsung_e9},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e10), samsung_e10},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e11), samsung_e11},
//        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e12), samsung_e12},
//        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e13), samsung_e13},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_te0), samsung_te0},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_te1), samsung_te1},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e14), samsung_e14},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_e15), samsung_e15},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_e16), samsung_e16},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_e17_C2), samsung_e17_C2},
        {DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(enable_te), enable_te},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_panel_width), samsung_panel_width},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0,  sizeof(samsung_panel_height), samsung_panel_height},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_panel_vinit), samsung_panel_vinit},
};

static struct dsi_cmd_desc samsung_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(enter_sleep), enter_sleep}
};

static struct dsi_cmd_desc samsung_display_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_on), display_on},
};

static char manufacture_id[2] = {0x04, 0x00}; 		/* DTYPE_DCS_READ */

static struct dsi_cmd_desc samsung_manufacture_id_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id), manufacture_id};

#define PWM_MIN                   30
#define PWM_DEFAULT               142
#define PWM_MAX                   255

#define BRI_SETTING_MIN                 30
#define BRI_SETTING_DEF                 142
#define BRI_SETTING_MAX                 255

#define AMOLED_NUM_LEVELS_C2 	ARRAY_SIZE(samsung_amoled_gamma_table_c2)

#define AMOLED_MIN_VAL 		30
#define AMOLED_MAX_VAL 		255
#define AMOLED_DEF_VAL 		(AMOLED_MIN_VAL + (AMOLED_MAX_VAL - \
								AMOLED_MIN_VAL) / 2)
#define AMOLED_LEVEL_STEP_C2 	((AMOLED_MAX_VAL - AMOLED_MIN_VAL) /  \
								(AMOLED_NUM_LEVELS_C2 - 1))
#define AMOLED_GAMMA_TABLE_SIZE 25+1

static const char samsung_amoled_gamma_table_c2[][AMOLED_GAMMA_TABLE_SIZE] = {
	/* level 10 */
//	{0xFA, 0x02, 0x55, 0x43, 0x58, 0x5A, 0x5A, 0x5A, 0x80, 0x80,
//		0x80, 0xBD, 0xC6, 0xBB, 0x96, 0xA6, 0x93, 0xBD, 0xCD, 0xC6,
//		0x00, 0x4C, 0x00, 0x44, 0x00, 0x57},
	/* level 20 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0x63, 0x63, 0x64, 0xA3, 0xA6,
		0xA1, 0xB6, 0xC2, 0xB2, 0x97, 0xAA, 0x9C, 0xBD, 0xC8, 0xC0,
		0x00, 0x5E, 0x00, 0x54, 0x00, 0x69},
	/* level 40 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0x8C, 0x8C, 0x8C, 0xA1, 0xAF,
		0x9D, 0xBF, 0xC8, 0xBC, 0x9A, 0xAF, 0xA4, 0xBB, 0xC3, 0xBA,
		0x00, 0x73, 0x00, 0x68, 0x00, 0x81},
	/* level 60 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0x91, 0x94, 0x90, 0xA7, 0xB5,
		0xA3, 0xBD, 0xCA, 0xC0, 0x96, 0xA8, 0x9D, 0xBA, 0xC2, 0xB8,
		0x00, 0x83, 0x00, 0x76, 0x00, 0x92},
	/* level 80 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0x9B, 0xA0, 0x99, 0xA6, 0xB3,
		0xA2, 0xBD, 0xCC, 0xC4, 0x96, 0xA5, 0x99, 0xBA, 0xC1, 0xB7,
		0x00, 0x8F, 0x00, 0x81, 0x00, 0xA0},
	/* level 100 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0xA3, 0xAC, 0xA1, 0xA5, 0xB2,
		0xA2, 0xC0, 0xD0, 0xC8, 0x96, 0xA3, 0x97, 0xB7, 0xBE, 0xB5,
		0x00, 0x9A, 0x00, 0x8B, 0x00, 0xAC},
	/* level 120 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0xA6, 0xB1, 0xA3, 0xAA, 0xB8,
		0xA9, 0xBF, 0xCE, 0xC7, 0x95, 0xA1, 0x95, 0xB6, 0xBC, 0xB4,
		0x00, 0xA3, 0x00, 0x94, 0x00, 0xB7},
	/* level 140 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0xA9, 0xB6, 0xA6, 0xAE, 0xBC,
		0xB0, 0xBD, 0xCC, 0xC4, 0x97, 0xA2, 0x95, 0xB4, 0xBA, 0xB1,
		0x00, 0xAC, 0x00, 0x9C, 0x00, 0xC1},
	/* level 160 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0xA5, 0xB2, 0xA1, 0xAD, 0xBC,
		0xB0, 0xBD, 0xCB, 0xC3, 0x95, 0xA0, 0x93, 0xB1, 0xB7, 0xAE,
		0x00, 0xB5, 0x00, 0xA5, 0x00, 0xCC},
	/* level 180 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0xAA, 0xB9, 0xA6, 0xAD, 0xBE,
		0xB2, 0xBE, 0xCA, 0xC3, 0x94, 0x9E, 0x92, 0xB1, 0xB7, 0xAD,
		0x00, 0xBC, 0x00, 0xAC, 0x00, 0xD5},
	/* level 200 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0xA6, 0xB6, 0xA2, 0xAC, 0xBD,
		0xB2, 0xBE, 0xC9, 0xC2, 0x93, 0x9E, 0x91, 0xAF, 0xB5, 0xAB,
		0x00, 0xC4, 0x00, 0xB3, 0x00, 0xDD},
	/* level 220 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0xAB, 0xBB, 0xA6, 0xAB, 0xBD,
		0xB2, 0xBC, 0xC7, 0xBF, 0x93, 0x9D, 0x90, 0xAF, 0xB4, 0xAB,
		0x00, 0xCA, 0x00, 0xB9, 0x00, 0xE5},
	/* level 240 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0xAB, 0xBD, 0xA7, 0xAF, 0xC1,
		0xB7, 0xBC, 0xC6, 0xBE, 0x92, 0x9D, 0x90, 0xAE, 0xB3, 0xA9,
		0x00, 0xD1, 0x00, 0xBF, 0x00, 0xED},
	/* level 260 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0xA8, 0xBA, 0xA3, 0xAE, 0xC0,
		0xB7, 0xBC, 0xC5, 0xBD, 0x92, 0x9D, 0x90, 0xAC, 0xB1, 0xA7,
		0x00, 0xD8, 0x00, 0xC5, 0x00, 0xF5},
	/* level 280 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0xAC, 0xBF, 0xA7, 0xAC, 0xC0,
		0xB7, 0xBE, 0xC6, 0xBE, 0x90, 0x9A, 0x8D, 0xAC, 0xB1, 0xA7,
		0x00, 0xDE, 0x00, 0xCB, 0x00, 0xFC},
	/* level 300 */
	{0xFA, 0x02, 0x55, 0x43, 0x58, 0xA9, 0xBC, 0xA4, 0xAC, 0xC0,
		0xB7, 0xBE, 0xC6, 0xBE, 0x91, 0x9B, 0x8E, 0xAB, 0xB0, 0xA6,
		0x00, 0xE4, 0x00, 0xD1, 0x01, 0x04},
};

static const char black_gamma[AMOLED_GAMMA_TABLE_SIZE] = {
	0xFA, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static char set_gamma[AMOLED_GAMMA_TABLE_SIZE] = {
	0xFA, 0x02, 0x10, 0x10, 0x10, 0xC5, 0xAB, 0xCD, 0xD7, 0xB9,
	0xD1, 0xDC, 0xD7, 0xDD, 0xBD, 0xC0, 0xBA, 0xCD, 0xC9, 0xC8,
	0x00, 0x8C, 0x00, 0x6F, 0x00, 0xA0};

static struct dsi_cmd_desc samsung_cmd_backlight_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(set_gamma), set_gamma},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(samsung_g1), samsung_g1},
};

static struct mipi_dsi_phy_ctrl samsung_dsi_cmd_mode_phy_db = {
	/* DSI_BIT_CLK at 250MHz, 2 lane, RGB888 */
	/* regulator */
	{0x03, 0x01, 0x01, 0x00},
	/* timing */
	//clk_rate:600MHz
	{0xAE, 0x8B, 0x1A, 0x00, 0x1C, 0x92, 0x1D, 0x8D,
	 0x1C, 0x03, 0x04},
	 /* phy ctrl */
	{0x7f, 0x00, 0x00, 0x00},
	 /* strength */
	{0xee, 0x02, 0x86, 0x00},
	/* pll control */
	{0x40, 0x84, 0xb9, 0xd9, 0x00, 0x50, 0x48, 0x63,
	 0x30, 0x07, 0x03,
	 0x05, 0x14, 0x03, 0x00, 0x00, 0x54, 0x06, 0x10, 0x04, 0x00},
};

#if defined (CONFIG_FB_MSM_MDP_ABL)
static struct gamma_curvy smd_gamma_tbl = {
       .gamma_len = 33,
       .bl_len = 8,
       .ref_y_gamma = {0, 1, 2, 4, 8, 16, 24, 33, 45, 59, 74, 94,
                       112, 133, 157, 183, 212, 244, 275, 314, 348,
                       392, 439, 487, 537, 591, 645, 706, 764, 831,
                       896, 963, 1024},
       .ref_y_shade = {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352,
                       384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704,
                       736, 768, 800, 832, 864, 896, 928, 960, 992, 1024},
       .ref_bl_lvl = {0, 141, 233, 317, 462, 659, 843, 1024},
       .ref_y_lvl = {0, 138, 218, 298, 424, 601, 818, 1024},
};
#endif

#if defined (CONFIG_MSM_AUTOBL_ENABLE)
static int acl_enable = 0;
char acl_cutoff_40[] = {
	0xC1, 0x47, 0x53, 0x13, 0x53, 0x00, 0x00, 0x01, 0xDF, 0x00,
	0x00, 0x03, 0x1F, 0x00, 0x00, 0x00, 0x05, 0x0E, 0x0F, 0x0F,
	0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x00
};
static char acl_on[] = {0xC0, 0x01};/* DTYPE_DCS_WRITE1 */
static char acl_off[] = {0xC0, 0x00};/* DTYPE_DCS_WRITE1 */
static struct dsi_cmd_desc samsung_acl_on_cmd[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(acl_cutoff_40), acl_cutoff_40},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,  sizeof(acl_on), acl_on},
};

static struct dsi_cmd_desc samsung_acl_off_cmd[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(acl_off), acl_off},
};
#endif

/* =============================================================================
 * SAMSUNG PANEL FUNCTION
 * =============================================================================
 */

static uint32 mipi_samsung_manufacture_id(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *rp, *tp;
	struct dsi_cmd_desc *cmd;
/* 	uint32 *lp; */

	tp = &panel_tx_buf;
	rp = &panel_rx_buf;
	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);

	cmd = &samsung_manufacture_id_cmd;
/*
//-	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 3);
//-	lp = (uint32 *)rp->data;
//-	PR_DISP_INFO("%s: manufacture_id=%x\n", __func__, *lp);
//-	return *lp;
*/
	return 0;
}

static unsigned char ville_shrink_pwm_c2(int val)
{
	int i;
	int level, frac, shrink_br = 255;
	unsigned int prev_gamma, next_gammma, interpolate_gamma;

	if(val == 0) {
		for (i = 0; i < AMOLED_GAMMA_TABLE_SIZE; ++i) {
			interpolate_gamma = black_gamma[i];
			set_gamma[i] = (char) interpolate_gamma;
		}
		return val;
	}

	val = clamp(val, BRI_SETTING_MIN, BRI_SETTING_MAX);

	if ((val >= BRI_SETTING_MIN) && (val <= BRI_SETTING_DEF)) {
			shrink_br = (val - BRI_SETTING_MIN) * (PWM_DEFAULT - PWM_MIN) /
		(BRI_SETTING_DEF - BRI_SETTING_MIN) + PWM_MIN;
	} else if (val > BRI_SETTING_DEF && val <= BRI_SETTING_MAX) {
			shrink_br = (val - BRI_SETTING_DEF) * (PWM_MAX - PWM_DEFAULT) /
		(BRI_SETTING_MAX - BRI_SETTING_DEF) + PWM_DEFAULT;
	} else if (val > BRI_SETTING_MAX)
			shrink_br = PWM_MAX;

	level = (shrink_br - AMOLED_MIN_VAL) / AMOLED_LEVEL_STEP_C2;
	frac = (shrink_br - AMOLED_MIN_VAL) % AMOLED_LEVEL_STEP_C2;

	for (i = 0; i < AMOLED_GAMMA_TABLE_SIZE - 2; ++i) {
		if (frac == 0 || level == 14) {
			interpolate_gamma = samsung_amoled_gamma_table_c2[level][i];
		} else {
			prev_gamma = samsung_amoled_gamma_table_c2[level][i];
			next_gammma = samsung_amoled_gamma_table_c2[level+1][i];
			interpolate_gamma = (prev_gamma * (AMOLED_LEVEL_STEP_C2 -
									frac) + next_gammma * frac) /
									AMOLED_LEVEL_STEP_C2;
		}
		set_gamma[i] = (char) interpolate_gamma;
	}

	/* special case for SMD gamma setting  */
	if(frac == 0 || level == 14) {
		set_gamma[24] = (char)(samsung_amoled_gamma_table_c2[level][24]);
		set_gamma[25] = (char)(samsung_amoled_gamma_table_c2[level][25]);
	} else {
		prev_gamma = samsung_amoled_gamma_table_c2[level][24] * 256 + samsung_amoled_gamma_table_c2[level][25];
		next_gammma = samsung_amoled_gamma_table_c2[level+1][24] * 256 + samsung_amoled_gamma_table_c2[level+1][25];
		interpolate_gamma = (prev_gamma * (AMOLED_LEVEL_STEP_C2 -
						frac) + next_gammma * frac) /
						AMOLED_LEVEL_STEP_C2;
		set_gamma[24] = interpolate_gamma / 256;
		set_gamma[25] = interpolate_gamma % 256;
	}

	return val;
}

static int mipi_cmd_samsung_blue_qhd_pt_init(void)
{
	int ret;

	PR_DISP_INFO("panel: mipi_cmd_samsung_qhd\n");

#if defined (CONFIG_FB_MSM_MDP_ABL)
	pinfo.panel_char = smd_gamma_tbl;
#endif
	pinfo.xres = 540;
	pinfo.yres = 960;
	pinfo.type = MIPI_CMD_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
	pinfo.width = 49;
	pinfo.height = 87;

	pinfo.lcdc.h_back_porch = 64;
	pinfo.lcdc.h_front_porch = 96;
	pinfo.lcdc.h_pulse_width = 32;
	pinfo.lcdc.v_back_porch = 16;
	pinfo.lcdc.v_front_porch = 16;
	pinfo.lcdc.v_pulse_width = 4;
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;
	pinfo.clk_rate = 482000000;
	pinfo.lcd.vsync_enable = TRUE;
	pinfo.lcd.hw_vsync_mode = TRUE;
	pinfo.lcd.refx100 = 6096; /* adjust refx100 to prevent tearing */
	pinfo.lcd.v_back_porch = 16;
	pinfo.lcd.v_front_porch = 16;
	pinfo.lcd.v_pulse_width = 4;


	pinfo.mipi.mode = DSI_CMD_MODE;
	pinfo.mipi.dst_format = DSI_CMD_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.t_clk_post = 0x0a;
	pinfo.mipi.t_clk_pre = 0x20;
	pinfo.mipi.stream = 0;	/* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_NONE;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.te_sel = 1; /* TE from vsycn gpio */
	pinfo.mipi.interleave_max = 1;
	pinfo.mipi.insert_dcs_cmd = TRUE;
	pinfo.mipi.wr_mem_continue = 0x3c;
	pinfo.mipi.wr_mem_start = 0x2c;

	pinfo.mipi.dsi_phy_db = &samsung_dsi_cmd_mode_phy_db;

	ret = mipi_villec2_device_register("mipi_samsung", &pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_QHD_PT);
	if (ret)
		PR_DISP_ERR("%s: failed to register device!\n", __func__);

	this_driver.driver.name = "mipi_samsung";
	return ret;
}

/* =============================================================================
 * COMMON FUNCTION
 * =============================================================================
 */

static inline void mipi_dsi_set_backlight(struct msm_fb_data_type *mfd)
{
	struct mipi_panel_info *mipi;

	mipi  = &mfd->panel_info.mipi;

	if (panel_type == PANEL_ID_VILLE_SAMSUNG_SG_C2)
		ville_shrink_pwm_c2(mfd->bl_level);

	mutex_lock(&mfd->dma->ov_mutex);

/* Remove the check first for impact MFG test. Command by adb to set backlight not work */
#if 0
	if (mdp4_overlay_dsi_state_get() <= ST_DSI_SUSPEND) {
		mutex_unlock(&mfd->dma->ov_mutex);
		return;
	}
#endif

	if (mfd->panel_info.type == MIPI_CMD_PANEL) {
		mdp4_dsi_cmd_dma_busy_wait(mfd);
		mdp4_dsi_blt_dmap_busy_wait(mfd);
		mipi_dsi_mdp_busy_wait(mfd);
	}

	if (panel_type == PANEL_ID_VILLE_SAMSUNG_SG_C2) {
		mipi_dsi_cmds_tx(mfd, &panel_tx_buf, samsung_cmd_backlight_cmds,
				ARRAY_SIZE(samsung_cmd_backlight_cmds));
	}
	mutex_unlock(&mfd->dma->ov_mutex);

	return;
}

static int villec2_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi  = &mfd->panel_info.mipi;

	if (mipi->mode == DSI_VIDEO_MODE) {
		/*
		//-mipi_dsi_cmds_tx(mfd, &panel_tx_buf, novatek_video_on_cmds,
		//-	ARRAY_SIZE(novatek_video_on_cmds));
		*/
		PR_DISP_ERR("%s: not support DSI_VIDEO_MODE!(%d)", __func__, mipi->mode);
	} else {
		if (!mipi_lcd_on) {
			mipi_dsi_cmd_bta_sw_trigger(); /* clean up ack_err_status */
			if (panel_type == PANEL_ID_VILLE_SAMSUNG_SG_C2) {
				printk(KERN_INFO "villec2_lcd_on PANEL_ID_VILLE_SAMSUNG_SG_C2\n");
				mipi_dsi_cmds_tx(mfd, &panel_tx_buf, samsung_cmd_on_cmds_c2,
					ARRAY_SIZE(samsung_cmd_on_cmds_c2));
			} else {
				PR_DISP_ERR("%s: panel_type is not supported!(%d)", __func__, panel_type);
			}
		}
		mipi_dsi_cmd_bta_sw_trigger(); /* clean up ack_err_status */

		mipi_samsung_manufacture_id(mfd);
	}
	mipi_lcd_on = 1;
	return 0;
}

static void villec2_display_on(struct msm_fb_data_type *mfd)
{
	mutex_lock(&mfd->dma->ov_mutex);

	if (mfd->panel_info.type == MIPI_CMD_PANEL) {
		mdp4_dsi_cmd_dma_busy_wait(mfd);
		mdp4_dsi_blt_dmap_busy_wait(mfd);
		mipi_dsi_mdp_busy_wait(mfd);
	}

	if (panel_type == PANEL_ID_VILLE_SAMSUNG_SG_C2)
		mipi_dsi_cmds_tx(mfd, &panel_tx_buf, samsung_display_on_cmds,
			ARRAY_SIZE(samsung_display_on_cmds));

	cur_bl_level = 0;

#if defined (CONFIG_MSM_AUTOBL_ENABLE)
	if (acl_enable) {
		mipi_dsi_cmds_tx(mfd, &panel_tx_buf, samsung_acl_on_cmd,
			ARRAY_SIZE(samsung_acl_on_cmd));
		acl_enable = 1;
		PR_DISP_INFO("%s acl enable", __func__);
	}
#endif
	mutex_unlock(&mfd->dma->ov_mutex);
}

static int villec2_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;
	if (!mipi_lcd_on)
		return 0;

	/*
	// Remove mutex temporarily
	//mutex_lock(&mfd->dma->ov_mutex);
	*/
	if (panel_type == PANEL_ID_VILLE_SAMSUNG_SG_C2) {
		mipi_dsi_cmds_tx(mfd, &panel_tx_buf, samsung_display_off_cmds,
				ARRAY_SIZE(samsung_display_off_cmds));
	}
	/*
	//mutex_unlock(&mfd->dma->ov_mutex);
	*/
	mipi_lcd_on = 0;
	return 0;
}

static void villec2_set_backlight(struct msm_fb_data_type *mfd)
{
	if (!mfd->panel_power_on || cur_bl_level == mfd->bl_level) {
		return;
	}

	mipi_dsi_set_backlight(mfd);

	cur_bl_level = mfd->bl_level;
	PR_DISP_DEBUG("%s+ bl_level=%d\n", __func__, mfd->bl_level);
}

#if defined (CONFIG_MSM_AUTOBL_ENABLE)
static int villec2_samsung_acl_enable(int on, struct msm_fb_data_type *mfd)
{
	static int first_time = 1;
	static unsigned long last_autobkl_stat = 0, cur_autobkl_stat = 0;

	/* if backlight is over 245, then disable acl */
	if(cur_bl_level > 245)
		cur_autobkl_stat = 8;
	else
		cur_autobkl_stat = on;

	if(cur_autobkl_stat == last_autobkl_stat)
		return 0;

	last_autobkl_stat = cur_autobkl_stat;
	mutex_lock(&mfd->dma->ov_mutex);
	if (mfd->panel_info.type == MIPI_CMD_PANEL) {
		mdp4_dsi_cmd_dma_busy_wait(mfd);
		mdp4_dsi_blt_dmap_busy_wait(mfd);
		mipi_dsi_mdp_busy_wait(mfd);
	}

	if (cur_autobkl_stat == 8 && !first_time) {
		if (panel_type == PANEL_ID_VILLE_SAMSUNG_SG_C2) {
			mipi_dsi_cmds_tx(mfd, &panel_tx_buf, samsung_acl_off_cmd,
				ARRAY_SIZE(samsung_acl_off_cmd));
			acl_enable = 0;
			PR_DISP_INFO("%s acl disable", __func__);
		}
	} else if (cur_autobkl_stat == 12) {
		if (panel_type == PANEL_ID_VILLE_SAMSUNG_SG_C2) {
			mipi_dsi_cmds_tx(mfd, &panel_tx_buf, samsung_acl_on_cmd,
				ARRAY_SIZE(samsung_acl_on_cmd));
			acl_enable = 1;
			PR_DISP_INFO("%s acl enable", __func__);
		}
	}
	first_time = 0;
	mutex_unlock(&mfd->dma->ov_mutex);
	return 0;
}
#endif


static int __devinit villec2_lcd_probe(struct platform_device *pdev)
{
	msm_fb_add_device(pdev);

	return 0;
}

static void villec2_lcd_shutdown(struct platform_device *pdev)
{
	mipi_dsi_panel_power(0);
}

static int mipi_villec2_device_register(const char* dev_name, struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	static int ch_used[3] = {0, 0, 0};
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc(dev_name, (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	villec2_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &villec2_panel_data,
		sizeof(villec2_panel_data));
	if (ret) {
		PR_DISP_ERR("%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		PR_DISP_ERR("%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}
	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int mipi_dsi_panel_power(const int on)
{
	static bool dsi_power_on = false;
	static struct regulator *v_lcm, *v_lcmio, *v_dsivdd;
	static bool bPanelPowerOn = false;
	int rc;

	const char * const lcm_str 	  = "8058_l12";
	const char * const lcmio_str  = "8901_lvs1";
	const char * const dsivdd_str = "8058_l16";

	PR_DISP_INFO("%s: state : %d\n", __func__, on);

	if (!dsi_power_on) {

		v_lcm = regulator_get(/* &msm_mipi_dsi1_device.dev */NULL,
				lcm_str);
		if (IS_ERR(v_lcm)) {
			PR_DISP_ERR("could not get %s, rc = %ld\n",
				lcm_str, PTR_ERR(v_lcm));
			return -ENODEV;
		}

		v_lcmio = regulator_get(/* &msm_mipi_dsi1_device.dev */NULL,
				lcmio_str);
		if (IS_ERR(v_lcmio)) {
			PR_DISP_ERR("could not get %s, rc = %ld\n",
				lcmio_str, PTR_ERR(v_lcmio));
			return -ENODEV;
		}

		v_dsivdd = regulator_get(/* &msm_mipi_dsi1_device.dev */NULL,
				dsivdd_str);
		if (IS_ERR(v_dsivdd)) {
			PR_DISP_ERR("could not get %s, rc = %ld\n",
				dsivdd_str, PTR_ERR(v_dsivdd));
			return -ENODEV;
		}

		rc = regulator_set_voltage(v_lcm, 3000000, 3000000);
		if (rc) {
			PR_DISP_ERR("%s#%d: set_voltage %s failed, rc=%d\n", __func__, __LINE__, lcm_str, rc);
			return -EINVAL;
		}

		rc = regulator_set_voltage(v_dsivdd, 1800000, 1800000);
		if (rc) {
			PR_DISP_ERR("%s#%d: set_voltage %s failed, rc=%d\n", __func__, __LINE__, dsivdd_str, rc);
			return -EINVAL;
		}

		rc = gpio_request(GPIO_LCM_RST_N, "LCM_RST_N");
		if (rc) {
			PR_DISP_ERR("%s:LCM gpio %d request failed, rc=%d\n", __func__,  GPIO_LCM_RST_N, rc);
			return -EINVAL;
		}
		gpio_direction_output(GPIO_LCM_RST_N, 0);
		dsi_power_on = true;
	}

	if (on) {
		PR_DISP_INFO("%s: on\n", __func__);
		rc = regulator_set_optimum_mode(v_lcm, 100000);
		if (rc < 0) {
			PR_DISP_ERR("set_optimum_mode %s failed, rc=%d\n", lcm_str, rc);
			return -EINVAL;
		}

		rc = regulator_set_optimum_mode(v_dsivdd, 100000);
		if (rc < 0) {
			PR_DISP_ERR("set_optimum_mode %s failed, rc=%d\n", dsivdd_str, rc);
			return -EINVAL;
		}

		rc = regulator_enable(v_dsivdd);
		if (rc) {
			PR_DISP_ERR("enable regulator %s failed, rc=%d\n", dsivdd_str, rc);
			return -ENODEV;
		}

		hr_msleep(1);
		rc = regulator_enable(v_lcmio);
		if (rc) {
			PR_DISP_ERR("enable regulator %s failed, rc=%d\n", lcmio_str, rc);
			return -ENODEV;
		}

		rc = regulator_enable(v_lcm);
		if (rc) {
			PR_DISP_ERR("enable regulator %s failed, rc=%d\n", lcm_str, rc);
			return -ENODEV;
		}

		if (!mipi_lcd_on) {
			hr_msleep(10);
			gpio_set_value(GPIO_LCM_RST_N, 1);
			hr_msleep(1);
			gpio_set_value(GPIO_LCM_RST_N, 0);
			hr_msleep(35);
			gpio_set_value(GPIO_LCM_RST_N, 1);
		}
		hr_msleep(60);
		bPanelPowerOn = true;
	} else {
		PR_DISP_INFO("%s: off\n", __func__);
		if (!bPanelPowerOn) return 0;
		hr_msleep(100);
		gpio_set_value(GPIO_LCM_RST_N, 0);
		hr_msleep(10);

		if (regulator_disable(v_lcm)) {
			PR_DISP_ERR("%s: Unable to enable the regulator: %s\n", __func__, lcm_str);
			return -EINVAL;
		}
		hr_msleep(5);
		if (regulator_disable(v_lcmio)) {
			PR_DISP_ERR("%s: Unable to enable the regulator: %s\n", __func__, lcmio_str);
			return -EINVAL;
		}

		if (panel_type == PANEL_ID_VILLE_SAMSUNG_SG_C2) {
			if (regulator_disable(v_dsivdd)) {
				PR_DISP_ERR("%s: Unable to enable the regulator: %s\n", __func__, dsivdd_str);
				return -EINVAL;
			}

			rc = regulator_set_optimum_mode(v_dsivdd, 100);
			if (rc < 0) {
				PR_DISP_ERR("%s: Unable to enable the regulator: %s\n", __func__, dsivdd_str);
				return -EINVAL;
			}
		}
		bPanelPowerOn = false;
	}
	return 0;
}

static int __init villec2_panel_init(void)
{
	if(panel_type != PANEL_ID_NONE)
		msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
	else
		printk(KERN_INFO "[DISP]panel ID= NONE\n");

	return 0;
}

static int __init villec2_panel_late_init(void)
{
	mipi_dsi_buf_alloc(&panel_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&panel_rx_buf, DSI_BUF_SIZE);

	if(panel_type == PANEL_ID_NONE) {
		PR_DISP_INFO("%s panel ID = PANEL_ID_NONE\n", __func__);
		return 0;
	}

	if (panel_type == PANEL_ID_VILLE_SAMSUNG_SG_C2) {
		mipi_cmd_samsung_blue_qhd_pt_init();
	}

	return platform_driver_register(&this_driver);
}

module_init(villec2_panel_init);
late_initcall(villec2_panel_late_init);
