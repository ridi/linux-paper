/*
 * RK29 ebook lut  epd_lut.h
 *
 * Copyright (C) 2010 RockChip, Inc.
 * Author: dlx@rock-chips.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef EPD_LUT_H
#define EPD_LUT_H
/*include*/
#include "spiflash/epd_spi_flash.h"


/*define*/
#define EPD_LUT_READ_FROM_SPI 1

#define LUT_SUCCESS (0)
#define LUT_ERROR (-1)


/* GET LUT MODE */
#define LUT_FROM_GPIO_SPI_FLASH		(0)
#define LUT_FROM_RK_SPI_FLASH		(1)
#define LUT_FROM_NAND_FLASH		(2)
#define LUT_FROM_WAVEFORM_FILE		(3)


enum epd_lut_type
{
	WF_TYPE_RESET=1,
	WF_TYPE_GRAY16,
	WF_TYPE_GRAY4,
	WF_TYPE_GRAY2,
	WF_TYPE_AUTO,
	WF_TYPE_A2,
	WF_TYPE_MAX,
};

enum epd_pvi_mode_type
{
	WF_MODE_RESET  = 0,
	WF_MODE_DU     = 1,
	WF_MODE_DU4    = 2,
	WF_MODE_GC16   = 3,
	WF_MODE_GL16   = 4,
	WF_MODE_GLR16  = 5,
	WF_MODE_GLD16  = 6,
	WF_MODE_A2     = 7,
	WF_MODE_MAX,
};

struct epd_lut_data
{
	unsigned int  frame_num;
	unsigned int * data;
};

//ebc lut op
struct epd_lut_ops
{
	int (*lut_get)(struct epd_lut_data *,enum epd_lut_type,bool,int,int);
};


/*api*/
//read lut from spi flash.
//cache lut data.
int epd_lut_from_gpio_spi_init(char *nand_buffer);
int epd_lut_from_rk_spi_init(char *nand_buffer);
int epd_lut_from_nand_init(char *temp_buffer);
int epd_lut_from_file_init(char *temp_buffer);
int epd_lut_from_array_init();
//register lut op
int epd_lut_op_register(struct epd_lut_ops *op);

//register spi flash get spi op api.
int epd_spi_flash_register(struct epd_spi_flash_info *epd_spi_flash);
extern char *get_waveform_version();


#endif

