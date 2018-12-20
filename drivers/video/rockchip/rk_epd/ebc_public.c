#include "ebc.h"

enum ebc_panel_type {
	V220_EINK_800x480    = 0,
	COLOR_EINK_800x600   = 1,
	V110_EINK_800x600    = 2,
	V220_EINK_800x600    = 3,
	V220_EINK_1024x758   = 4,
	V220_EINK_1024x768   = 5,
	V220_EINK_1200x720   = 6,
	V110_EINK_1200x825   = 7,
	V220_EINK_1200x825   = 8,
	V220_EINK_1448x1072  = 9,
	V110_EINK_1600x1200  = 10,
};

struct ebc_panel support_panels[] =
{
	[V220_EINK_800x480] = {
		.width     = 800,  .height     = 480,   .frame_rate = 85,
		.vir_width = 896,  .vir_height = 600,
		.fb_width  = 800,  .fb_height  = 600,
		.hsync_len = 2,    .hstart_len = 3,     .hend_len = 0,
		.vsync_len = 4,    .vstart_len = 0,     .vend_len = 60,
		.gdck_sta  = 0,    .lgonl      = 0,
		.color_panel = 0,  .rotate     = 270,
	},

	[COLOR_EINK_800x600] = {
		.width     = 1808, .height     = 800,  .frame_rate = 85,
		.vir_width = 1920, .vir_height = 800,
		.fb_width  = 600,  .fb_height  = 800,
		.hsync_len = 15,   .hstart_len = 10,    .hend_len = 0,
		.vsync_len = 4,    .vstart_len = 0,     .vend_len = 4,
		.gdck_sta  = 0,    .lgonl      = 0,
		.color_panel = 1,  .rotate     = 0,
	},

	[V110_EINK_800x600] = {
		.width     = 800,  .height     = 600,  .frame_rate = 50,
		.vir_width = 896,  .vir_height = 600,
		.fb_width  = 800,  .fb_height  = 600,
		.hsync_len = 2,    .hstart_len = 3,    .hend_len = 0,
		.vsync_len = 4,    .vstart_len = 0,    .vend_len = 1,
		.gdck_sta  = 0,    .lgonl      = 0,
		.color_panel = 0,  .rotate = 270,
	},

	[V220_EINK_800x600] = {
		.width     = 800,  .height     = 600,  .frame_rate = 85,
		.vir_width = 800,  .vir_height = 600,
		.fb_width  = 800,  .fb_height  = 600,
		.hsync_len = 2,    .hstart_len = 4,    .hend_len = 43,
		.vsync_len = 1,    .vstart_len = 4,    .vend_len = 8,
		.gdck_sta  = 1,    .lgonl      = 204,
		.color_panel = 0,  .rotate = 270,
	},

	[V220_EINK_1024x758] = {
		.width     = 1024, .height     = 758,  .frame_rate = 85,
		.vir_width = 1024, .vir_height = 758,
		.fb_width  = 1024, .fb_height  = 758,
		.hsync_len = 6,    .hstart_len = 6,    .hend_len = 38,
		.vsync_len = 2,    .vstart_len = 4,    .vend_len = 5,
		.gdck_sta  = 4,    .lgonl      = 262,
		.color_panel = 0,  .rotate = 90,
	},

	[V220_EINK_1024x768] = {
		.width     = 1024, .height     = 768,  .frame_rate = 85,
		.vir_width = 1024, .vir_height = 768,
		.fb_width  = 1024, .fb_height  = 768,
		.hsync_len = 6,    .hstart_len = 6,    .hend_len = 38,
		.vsync_len = 2,    .vstart_len = 4,    .vend_len = 5,
		.gdck_sta  = 4,    .lgonl      = 262,
		.color_panel = 0,  .rotate     = 270,
	},

	[V220_EINK_1200x720] = {
		.width     = 1200, .height     = 720,  .frame_rate = 85,
		.vir_width = 1200, .vir_height = 720,
		.fb_width  = 1200, .fb_height  = 720,
		.hsync_len = 6,    .hstart_len = 6,    .hend_len = 38,
		.vsync_len = 2,    .vstart_len = 4,    .vend_len = 5,
		.gdck_sta  = 4,    .lgonl      = 262,
		.color_panel = 0,  .rotate = 270,
	},

	[V110_EINK_1200x825] = {
		.width     = 1200, .height     = 825,  .frame_rate = 50,
		.vir_width = 1280, .vir_height = 825,
		.fb_width  = 1200, .fb_height  = 825,
		.hsync_len = 10,   .hstart_len = 6,    .hend_len = 69,
		.vsync_len = 4,    .vstart_len = 4,    .vend_len = 10,
		.gdck_sta  = 0,    .lgonl      = 301,
		.color_panel = 0,  .rotate     = 270,
	},

	[V220_EINK_1200x825] = {
		.width     = 1200, .height     = 825,   .frame_rate = 85,
		.vir_width = 1280, .vir_height = 825,
		.fb_width  = 1200, .fb_height  = 825,
		.hsync_len = 15,   .hstart_len = 10,    .hend_len = 0,
		.vsync_len = 4,    .vstart_len = 0,     .vend_len = 30,
		.gdck_sta  = 0,    .lgonl      = 0,
		.color_panel = 0,  .rotate     = 270,
	},

	[V220_EINK_1448x1072] = {
		.width     = 1448, .height     = 1072,  .frame_rate = 85,
		.vir_width = 1448, .vir_height = 1072,
		.fb_width  = 1448, .fb_height  = 1072,
		.hsync_len = 14,   .hstart_len = 8,     .hend_len = 51,
		.vsync_len = 2,    .vstart_len = 4,     .vend_len = 4,
		.gdck_sta  = 100,  .lgonl      = 281,
		.color_panel = 0,  .rotate     = 90,
	},

	[V110_EINK_1600x1200] = {
		.width     = 1600, .height     = 1200,  .frame_rate = 85,
		.vir_width = 1600, .vir_height = 1200,
		.fb_width  = 1600, .fb_height  = 1200,
		.hsync_len = 6,    .hstart_len = 4,     .hend_len = 13,
		.vsync_len = 1,    .vstart_len = 4,     .vend_len = 7,
		.gdck_sta  = 2,    .lgonl      = 400,
		.color_panel = 0,  .rotate     = 270,
	},
};

/* NOTE: the panel's width must 8 align */
void set_epd_info(struct ebc_panel *panel,
		  struct ebc_clk *epd_clk,
		  int *width, int *height)
{
#ifdef CONFIG_V110_EINK_800X600
	if (panel)
		*panel = support_panels[V110_EINK_800x600];
	if(epd_clk)
		epd_clk->pixclock = 10000000;
	if(width)
		*width = 800;
	if(height)
		*height = 600;
#elif defined(CONFIG_V220_EINK_800X600)
	if (panel)
		*panel = support_panels[V220_EINK_800x600];
	if(epd_clk)
		epd_clk->pixclock = 13000000;
	if(width)
		*width = 800;
	if(height)
		*height = 600;
#elif defined(CONFIG_V220_EINK_1024X768)
	if (panel)
		*panel = support_panels[V220_EINK_1024x768];
	if(epd_clk)
		epd_clk->pixclock = 20000000;
	if(width)
		*width = 1024;
	if(height)
		*height = 768;
#elif defined(CONFIG_V220_EINK_1448x1072)
	/*
	 * NOTE: if you want to carry this screen, you must ensure
	 * the DDR frequency would reach to 400MHz, otherwhise there
	 * would a serious problem about frame shifting.
	 */
	if (panel)
		*panel = support_panels[V220_EINK_1448x1072];
	if(epd_clk)
		epd_clk->pixclock = 40000000;
	if(width)
		*width = 1448;
	if(height)
		*height = 1072;
#elif defined(CONFIG_V220_EINK_1024X758)
	if (panel)
		*panel = support_panels[V220_EINK_1024x758];
	if(epd_clk)
		epd_clk->pixclock = 20000000;
	if(width)
		*width = 1024;
	if(height)
		*height = 758;
#elif defined(CONFIG_V220_EINK_1200X720)
	if (panel)
		*panel = support_panels[V220_EINK_1200x720];
	if(epd_clk)
		epd_clk->pixclock = 25000000;
	if(width)
		*width = 1200;
	if(height)
		*height = 720;
#elif defined(CONFIG_V110_EINK_1200X825)
	if (panel)
		*panel = support_panels[V110_EINK_1200x825];
	if(epd_clk)
		epd_clk->pixclock = 16250000;
	if(width)
		*width = 1200;
	if(height)
		*height = 825;
#elif defined(CONFIG_V220_EINK_1200X825)
	if (panel)
		*panel = support_panels[V220_EINK_1200x825];
	if(epd_clk)
		epd_clk->pixclock = 25000000;
	if(width)
		*width = 1200;
	if(height)
		*height = 825;
#elif defined(CONFIG_V220_EINK_800X480)
	if (panel)
		*panel = support_panels[V220_EINK_800x480];
	if(epd_clk)
		epd_clk->pixclock = 15000000;
	if(width)
		*width = 800;
	if(height)
		*height = 480;
#elif defined(CONFIG_V110_EINK_1600X1200)
	if (panel)
		*panel = support_panels[V110_EINK_1600x1200];
	if(epd_clk)
		epd_clk->pixclock = 44000000;
	if(width)
		*width = 1600;
	if(height)
		*height = 1200;
#elif defined(CONFIG_COLOR_EINK_800X600)
	if (panel)
		*panel = support_panels[COLOR_EINK_800x600];
	if(epd_clk)
		epd_clk->pixclock = 38000000;
	if(width)
		*width = 600;
	if(height)
		*height =800;
#endif
}

void ebc_io_ctl_hook(unsigned int cmd, struct ebc_buf_info *info)
{
	switch (cmd) {
	case GET_EBC_BUFFER:
		/*
		 * Here you can check the struct ebc_buf_info.
		 *   buf_info.offset,
		 *   buf_info.height,      buf_info.width,
		 *   buf_info.vir_width,   buf_info.vir_height,
		 *   buf_info.fb_width,    buf_info.fb_height,
		 *   buf_info.color_panel, buf_info.rotate,
		 */
		break;
	case SET_EBC_SEND_BUFFER:
		/*
		 * Here you can translate struct ebc_buf_info to struct ebc_buf_s
		 *   struct ebc_buf_s *buf;
		 *   char *temp_addr;
		 *   temp_addr = ebc_phy_buf_base_get() + buf_info.offset;
		 *   buf = ebc_find_buf_by_phy_addr(temp_addr);
		 * then check the number of struct ebc_buf_s
		 *   buf->buf_mode,
		 *   buf->win_x1, buf->win_x2,
		 *   buf->win_y1, buf->win_y2,
		 */
		break;
	default:
		break;
	}
}

/*
 * The bellow functions are provided the personalized configuration,
 * customs could adjust them though the kernel make menuconfig.
 *
 *	support_pvi_waveform()
 *
 *	get_lut_position()
 *
 *	set_end_display()
 *
 *	support_bootup_ani()
 *	is_bootup_ani_loop()
 *	get_bootup_ani_mode()
 *	get_bootup_logo_cycle()
 *
 *	is_need_show_lowpower_pic()
 *
 *	support_tps_3v3_always_alive()
 */
int support_pvi_waveform()
{
#ifdef CONFIG_PVI_WAVEFORM
	return SPI_WAVEFORM;
#else
	return NAND_WAVEFORM;
#endif
}

int get_lut_position()
{
#ifdef CONFIG_WAVEFORM_FROM_GPIO_SPI
	return LUT_FROM_GPIO_SPI_FLASH;
#elif defined(CONFIG_WAVEFORM_FROM_RK_SPI)
	return LUT_FROM_RK_SPI_FLASH;
#elif defined(CONFIG_WAVEFORM_FROM_NAND_FLASH)
	return LUT_FROM_NAND_FLASH;
#elif defined(CONFIG_WAVEFORM_FROM_WAVEFORM_FILE)
	return LUT_FROM_WAVEFORM_FILE;
#endif
}

int set_end_display()
{
#ifdef CONFIG_END_PICTURE
	return END_PICTURE;
#else
	return END_RESET;
#endif
}

int get_bootup_logo_cycle(void)
{
	return CONFIG_EBC_ANI_CYC_TIME;
}

int is_bootup_ani_loop(void)
{
#if CONFIG_EBC_BOOT_ANI_LOOP
	return 1;
#else
	return 0;
#endif
}

int is_need_show_lowpower_pic(void)
{
#if CONFIG_SHOW_BATLOW_PIC
	return 1;
#else
	return 0;
#endif
}

int support_bootup_ani(void)
{
#if CONFIG_EBC_BOOT_ANI
	return 1;
#else
	return 0;
#endif
}

int support_double_thread_calcu(void)
{
#if CONFIG_DOUBLE_THREAD_CALCU
	return 1;
#else
	return 0;
#endif
}

int get_bootup_ani_mode(void)
{
#if CONFIG_EBC_BOOT_ANI_A2
	return EPD_A2;
#elif CONFIG_EBC_BOOT_ANI_PART
	return EPD_PART;
#elif  CONFIG_EBC_BOOT_ANI_FULL
	return EPD_FULL;
#elif  CONFIG_EBC_BOOT_ANI_DIR
	return EPD_DIRECT_PART;
#else
	return EPD_FULL;
#endif
}

int support_tps_3v3_always_alive(void)
{
#if CONFIG_EPD_PMIC_VccEink_ALWAYS_ALIVE
	return 1;
#else
	return 0;
#endif
}

int map_to_pvi_mode(enum epd_lut_type lut_type)
{
	switch (lut_type) {
	case WF_TYPE_GRAY16:
#if CONFIG_GRAY16_TO_GC16
		return WF_MODE_GC16;
#elif CONFIG_GRAY16_TO_GL16
		return WF_MODE_GL16;
#elif CONFIG_GRAY16_TO_GLR16
		return WF_MODE_GLR16;
#elif CONFIG_GRAY16_TO_GLD16
		return WF_MODE_GLD16;
#else
		return WF_MODE_GL16;
#endif

	case WF_TYPE_AUTO:
#if CONFIG_AUTO_TO_GC16
		return WF_MODE_GC16;
#elif CONFIG_AUTO_TO_GL16
		return WF_MODE_GL16;
#elif CONFIG_AUTO_TO_GLR16
		return WF_MODE_GLR16;
#elif CONFIG_AUTO_TO_GLD16
		return WF_MODE_GLD16;
#else
		return WF_MODE_GL16;
#endif
	}
}
