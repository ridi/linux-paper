/*
 * RK29 ebook spi falsh driver epd_spi_falsh.c
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
/*include*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/jiffies.h>
#include <asm/types.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#if defined(CONFIG_PVI_WAVEFORM)
#include <linux/regulator/machine.h>
#include <mach/gpio.h>
#endif

#include "einkwf.h"
#include "eink_waveform.h"
#include "epd_spi_flash.h"
#include "../epd_lut.h"

/*define.*/
#if EPD_SPI_DEBUG
#define epd_spi_printk(fmt, args...)  printk(KERN_INFO "ebc " "%s(%d): " fmt, __FUNCTION__, __LINE__, ##args)
#else
#define epd_spi_printk(fmt, args...)
#endif
#if defined(CONFIG_PVI_WAVEFORM)
/* SPI Flash API */
#define SFM_ID			0x9F
/* waveform */
#define DEFAULT_WFM_ADDR	0x00886
#define EINK_WAVEFORM_FILESIZE	262144  // 256K..
#define WAVEFORM_VERSION_STRING_MAX 64

#define WF_UPD_MODES_07		7       // V220 210 dpi85Hz modes
#define WF_UPD_MODES_18		18
#define WF_UPD_MODES_19		19
#define WF_UPD_MODES_24		24
#define WF_UPD_MODES_25		25

/* vcom */
#define BCD_MODULE_MFG_OFFSET	9
#define WAVEFORM_AA_VCOM_SHIFT	250	// mV
#define mV_to_uV(mV)	((mV) * 1000)
#define uV_to_mV(uV)	((uV) / 1000)
#define V_to_uV(V)	(mV_to_uV((V) * 1000))
#define uV_to_V(uV)	(uV_to_mV(uV) / 1000)

/* panel */
#define PNL_BASE_PART_NUMBER	0x00
#define PNL_SIZE_PART_NUMBER	16

#define PANEL_ID_UNKNOWN	"????_???_??_???"
#define PNL_SIZE_ID_STR		32
#define PNL_BASE		0x00
#define PNL_SIZE		256
#define PNL_LAST		(PNL_SIZE - 1)
#define PNL_ADDR		0x30000     // Start of panel data.
#define PNL_BASE_WAVEFORM	0x20
#define PNL_SIZE_WAVEFORM	23

struct panel_info {
	struct panel_addrs *addrs;
	int  vcom_uV;
	long computed_checksum;
	long embedded_checksum;
	struct eink_waveform_info_t *waveform_info;
	char human_version[WAVEFORM_VERSION_STRING_MAX];
	char version[WAVEFORM_VERSION_STRING_MAX];
	char bcd[PNL_SIZE_BCD_STR];
	char id[PNL_SIZE_ID_STR];
};
static struct panel_info *panel_info_cache = NULL;

extern int tps65185_vcom_set(int vcom_mv);
#define EINK_CHECKSUM(c1, c2)	(((c2) << 16) | (c1))
#endif

/*struct.*/

static struct epd_spi_flash_info spi_flash_info;
static bool   spi_registered = false;

static void Delay100cyc(u32 count)
{
	u16 i;

	while (count--)
		for (i = 0; i < 23; i++);
}

static u32 SpiFlashWaitBusy(struct spi_device *flashdev)
{
	u8 cmd[1];
	u8 status=0xff;
	u32 i;
	
	for (i=0; i<500000; i++)
	{
		Delay100cyc(100);
		cmd[0] = 0x05;
		spi_write_then_read(flashdev, cmd, 1, &status, 1);
		if ((status & 0x01) == 0)		
			return 0;
	}
	return 1;
}

static u32 SPIFlashRead(void *flashdev, u32 addr, u8 *pdata, u32 len) 
{    
	u8 cmd[4];
	u32 ReadLen;
	u32 ret = 0;
	u32 data = (u32)pdata;
   
	epd_spi_printk("enter SPIFlashRead flashdev = %x addr = 0x%x pData = %x, len = %d.\n", (int)flashdev, addr, (int)pdata, len);

	while (len > 0)
	{
		ReadLen = (len > SPI_PAGE_SIZE)? SPI_PAGE_SIZE : len;     

		cmd[0] = READ_DATA;
		cmd[1] = addr>>16 & 0xff;
		cmd[2] = addr>>8 & 0xff;
		cmd[3] = addr & 0xff;
		ret = spi_write_then_read((struct spi_device *)flashdev, cmd, 4, (u8*)data, ReadLen);
		if( ret )
		{
			printk("spi_write_then_read err.\n");
			return 1;
		}
		data += ReadLen;
		len -= ReadLen;       
		addr += ReadLen;
	}

	return 0;
}

static u32 SPIFlashWrite(void *flashdev, u32 addr, u8 *pdata, u32 len) 
{   
	u8 data[20];
	u32 writeLen; 
	u32 ret=0;

	epd_spi_printk("enter SPIFlashWrite.\n");

	while (len > 0)      
	{      
		writeLen = SPI_PAGE_SIZE - (addr % SPI_PAGE_SIZE);	
		writeLen = (len > writeLen)? writeLen : len;
		data[0] = WRITE_ENABLE;    //write enable	    

		ret = spi_write_then_read((struct spi_device *)flashdev, (u8*)data, 1, NULL, 0);

		if (0!=SpiFlashWaitBusy((struct spi_device *)flashdev))
		{
			epd_spi_printk("SpiFlashWaitBusy err.\n");
			ret=1;
		}

		data[0] = BYTE_WRITE;    //byte program
		data[1] = addr>>16 & 0xff;
		data[2] = addr>>8 & 0xff;
		data[3] = addr & 0xff;

		memcpy(&data[4], pdata, writeLen);

		ret = spi_write_then_read((struct spi_device *)flashdev, (u8*)data, writeLen+4, NULL, 0 );      
		if(ret)
		{
			epd_spi_printk("spi_write_then_read err.\n");
			return 1;
		}
		pdata = (u8*)((u32)pdata + writeLen);
		addr = addr+writeLen;      
		len -= writeLen;

		Delay100cyc(30);  //大于100ns  4.333us*30=130us
		if (0!=SpiFlashWaitBusy((struct spi_device *)flashdev))
		{
			epd_spi_printk("SpiFlashWaitBusy err.\n");
			ret=1;
		}
	}

	data[0] = WRITE_DISABLE;    //write disable
	spi_write_then_read((struct spi_device *)flashdev, data, 1, NULL, 0);

	return 0;
}

static u32 Sector_Erase(void *flashdev, u32 addr) 
{    
	u8 data[4];

	epd_spi_printk("enter Sector_Erase.\n");

	data[0] = 0x06;    //write enable     
	spi_write_then_read((struct spi_device *)flashdev, data, 1, NULL, 0);

	data[0] = 0xd8;   // 块擦除
	data[1] = addr>>16 & 0xff;
	data[2] = addr>>8 & 0xff;
	data[3] = addr & 0xff;

	spi_write_then_read((struct spi_device *)flashdev, data, 4, NULL, 0 );

	if (0!=SpiFlashWaitBusy((struct spi_device *)flashdev))
	{
		epd_spi_printk("SpiFlashWaitBusy err.\n");
		return 1;
	}

	data[0] = 0x04;    //write disable
	spi_write_then_read((struct spi_device *)flashdev, data, 1, NULL, 0);

	return 0;

}

static int spi_write_data(void *flashdev, int addr,char *buf,int len)
{
	char *pagedata=NULL;
	int earse_count=0;
	u32 pageaddr=0;
	u32 pagelen=0;
	int temp_addr;
	int ret;

	pagedata = (char*)kmalloc(FLASH_PAGE_SIZE, GFP_KERNEL);
	if(pagedata<0)
	{
		epd_spi_printk("spi_flash_write kmalloc failed. \n");
		return -1;
	} 
	
	while(len>=FLASH_PAGE_SIZE){  	
		pagelen = addr % FLASH_PAGE_SIZE;
		if(pagelen)
			pageaddr=addr - pagelen;
		else
			pageaddr=addr;
		temp_addr=pageaddr;
		ret = SPIFlashRead(flashdev, pageaddr, pagedata, FLASH_PAGE_SIZE);
		if(ret != 0)
		{
			epd_spi_printk("SPIFlashRead err.\n");  
			return -1;
		}
		for(earse_count=0;earse_count<FLASH_PAGE_SIZE/FLASH_PAGE_EARSE;earse_count++){
		Sector_Erase(flashdev, pageaddr);  
		pageaddr=pageaddr+FLASH_PAGE_EARSE;
		}    
		SPIFlashWrite(flashdev, addr, (u8*)buf, FLASH_PAGE_SIZE); 
		if(pagelen)	
		SPIFlashWrite(flashdev, temp_addr, pagedata, pagelen); 
		buf=buf+FLASH_PAGE_SIZE;
		len -= FLASH_PAGE_SIZE;
		addr = temp_addr+FLASH_PAGE_SIZE;        
	}	
	
	if(len){ 	
		pagelen = addr % FLASH_PAGE_SIZE;
		if(pagelen)
			pageaddr=addr -pagelen;
		else
			pageaddr=addr;
		
		temp_addr=pageaddr;
		ret = SPIFlashRead(flashdev, pageaddr, pagedata, FLASH_PAGE_SIZE);
		if(ret != 0)
		{
			epd_spi_printk("SPIFlashRead err.\n");    
			return -1;
		}
		
		for(earse_count=0;earse_count<FLASH_PAGE_SIZE/FLASH_PAGE_EARSE;earse_count++){
			Sector_Erase(flashdev, pageaddr);  
			pageaddr=pageaddr+FLASH_PAGE_EARSE;
		} 
		SPIFlashWrite(flashdev, addr, (u8*)buf, len); 
		if(pagelen)	
			SPIFlashWrite(flashdev, temp_addr, pagedata, pagelen); 
		SPIFlashWrite(flashdev, addr+len, pagedata+pagelen+len, FLASH_PAGE_SIZE-pagelen-len); 
	}
	
	kfree(pagedata);
	return 0;
}
#if defined(CONFIG_PVI_WAVEFORM)
/*
 * CRC-32 algorithm from:
 *  <http://glacier.lbl.gov/cgi-bin/viewcvs.cgi/dor-test/crc32.c?rev=HEAD>
 */

/* Table of CRCs of all 8-bit messages. */
static unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
static int crc_table_computed = 0;

/* Make the table for a fast CRC. */
static void make_crc_table(void)
{
	unsigned long c;
	int n, k;

	for (n = 0; n < 256; n++) {
		c = (unsigned long) n;
		for (k = 0; k < 8; k++) {
			if (c & 1)
				c = 0xedb88320L ^ (c >> 1);
			else
				c = c >> 1;
		}
		crc_table[n] = c;
	}
	crc_table_computed = 1;
}

/*
 * Update a running crc with the bytes buf[0..len-1] and return
 * the updated crc. The crc should be initialized to zero. Pre- and
 * post-conditioning (one's complement) is performed within this
 * function so it shouldn't be done by the caller. Usage example:
 *
 *   unsigned long crc = 0L;
 *
 *   while (read_buffer(buffer, length) != EOF) {
 *     crc = update_crc(crc, buffer, length);
 *   }
 *   if (crc != original_crc) error();
 */
static unsigned long update_crc(unsigned long crc, unsigned char *buf, int len)
{
	unsigned long c = crc ^ 0xffffffffL;
	int n;

	if (!crc_table_computed)
		make_crc_table();
	for (n = 0; n < len; n++)
		c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);

	return c ^ 0xffffffffL;
}

/* Return the CRC of the bytes buf[0..len-1]. */
static unsigned long crc32(unsigned char *buf, int len)
{
	return update_crc(0L, buf, len);
}

/* Return the sum of the bytes buf[0..len-1]. */
unsigned sum32(unsigned char *buf, int len)
{
	unsigned c = 0;
	int n;

	for (n = 0; n < len; n++)
		c += buf[n];

	return c;
}

/* Return the sum of the bytes buf[0..len-1]. */
unsigned char sum8(unsigned char *buf, int len)
{
	unsigned char c = 0;
	int n;

	for (n = 0; n < len; n++)
		c += buf[n];

	return c;
}

static bool panel_data_valid(char *panel_data)
{
	bool result = false;

	if ( panel_data )
	{
		if ( strchr(panel_data, PNL_CHAR_UNKNOWN) )
		{
			printk(KERN_ERR "Unrecognized values in panel data\n");
			pr_debug("panel data = %s\n", panel_data);
		}
		else
			result = true;
	}

	return ( result );
}

enum panel_data_characters
{
	zero = 0x0, one, two, three, four, five, six, seven, eight, nine,
	underline = 0x0a, dot = 0x0b, negative = 0x0c,
	_a = 0xcb, _b, _c, _d, _e, _f, _g, _h, _i, _j, _k, _l, _m, _n,
	_o, _p, _q, _r, _s, _t, _u, _v, _w, _x, _y, _z,

	_A = 0xe5, _B, _C, _D, _E, _F, _G, _H, _I, _J, _K, _L, _M, _N,
	_O, _P, _Q, _R, _S, _T, _U, _V, _W, _X, _Y, _Z
};
typedef enum panel_data_characters panel_data_characters;
static void panel_data_translate(u8 *buffer, int to_read) {
	int i = 0;

	for (i = 0; i < to_read; i++) {
		if (buffer[i] >= _a && buffer[i] <= _z) {
			buffer[i] = 'a' + (buffer[i] - _a);
		} else if (buffer[i] >= _A && buffer[i] <= _Z) {
			buffer[i] = 'A' + (buffer[i] - _A);
		} else if (/* buffer[i] >= zero && */ buffer[i] <= nine) {
			buffer[i] = '0' + (buffer[i] - zero);
		} else if (buffer[i] == underline) {
			buffer[i] = '_';
		} else if (buffer[i] == dot) {
			buffer[i] = '.';
		} else if (buffer[i] == negative) {
			buffer[i] = '-';
		} else {
			buffer[i] = PNL_CHAR_UNKNOWN;
		}
	}
}

/* wavefrom info */
void eink_get_waveform_info(u8 *wf_buffer, struct eink_waveform_info_t *info)
{
	struct waveform_data_header *header = (struct waveform_data_header *)wf_buffer;
	if ( info )
	{
		info->waveform.version		= header->wf_version;
		info->waveform.subversion	= header->wf_subversion;
		info->waveform.type		= header->wf_type;
		info->waveform.run_type		= header->run_type;
		info->fpl.platform		= header->fpl_platform;
		info->fpl.size			= header->panel_size;
		info->fpl.adhesive_run_number	= header->mode_version; /* xd4: header->fpl_lot */
		info->waveform.mode_version	= header->mode_version;
		info->waveform.mfg_code		= header->amepd_part_number;
		info->waveform.vcom_shift	= header->vcom_shifted;

		if (info->waveform.type == EINK_WAVEFORM_TYPE_WR) {
			/* WR spec changes the definition of the byte at 0x16 */
			info->waveform.revision	= header->wf_revision;
		} else {
			info->waveform.tuning_bias = header->wf_revision;
			info->waveform.revision = 0;
		}

		info->waveform.fpl_rate		= header->frame_rate;

		info->fpl.lot			= header->fpl_lot;

		info->checksum			= header->checksum;
		info->filesize			= header->file_length;
		info->waveform.serial_number	= header->serial_number;

		/* XWIA is only 3 bytes */
		info->waveform.xwia = header->xwia;
		info->waveform.xwia &= 0xFFFFFF;

		if ( 0 == info->filesize ) {
			info->checksum = EINK_CHECKSUM(header->cs1, header->cs2);
			info->waveform.parse_wf_hex  = false;
		} else {
			info->waveform.parse_wf_hex  = false;
		}

		epd_spi_printk(   "\n"
				" Waveform version:  0x%02X\n"
				"       subversion:  0x%02X\n"
				"             type:  0x%02X (v%02d)\n"
				"         run type:  0x%02X\n"
				"     mode version:  0x%02X\n"
				"      tuning bias:  0x%02X\n"
				"       frame rate:  0x%02X\n"
				"       vcom shift:  0x%02X\n"
				"\n"
				"     FPL platform:  0x%02X\n"
				"              lot:  0x%04X\n"
				"             size:  0x%02X\n"
				" adhesive run no.:  0x%02X\n"
				"\n"
				"        File size:  0x%08lX\n"
				"         Mfg code:  0x%02X\n"
				"       Serial no.:  0x%08lX\n"
				"         Checksum:  0x%08lX\n",

				info->waveform.version,
				info->waveform.subversion,
				info->waveform.type,
				info->waveform.revision,
				info->waveform.run_type,
				info->waveform.mode_version,
				info->waveform.tuning_bias,
				info->waveform.fpl_rate,
				info->waveform.vcom_shift,

				info->fpl.platform,
				info->fpl.lot,
				info->fpl.size,
				info->fpl.adhesive_run_number,

				info->filesize,
				info->waveform.mfg_code,
				info->waveform.serial_number,
				info->checksum);
	}
}

static void panel_get_format(struct panel_info *panel, struct waveform_data_header *header)
{
        switch (header->mode_version) {
        case WF_UPD_MODES_07:
                panel->addrs = &panel_mode_07.addrs;
                break;
        case WF_UPD_MODES_18:
                panel->addrs = &panel_mode_18.addrs;
                break;
        case WF_UPD_MODES_19:
                panel->addrs = &panel_mode_19.addrs;
                break;
        case WF_UPD_MODES_24:
                panel->addrs = &panel_mode_24.addrs;
                break;
        case WF_UPD_MODES_25:
                panel->addrs = &panel_mode_25.addrs;
                break;
        default:
                printk(KERN_ALERT "%s: Unknown panel flash format (0x%02x). Add a definition for the waveform you are trying to use.\n",
                       __func__, header->mode_version);
        }
}

bool panel_flash_present(void)
{
	return ((NULL != spi_flash_info.dev) && spi_registered);
}

static int panel_read_from_flash(unsigned long addr, unsigned char *data, unsigned long size)
{
	int ret;

	ret = SPIFlashRead(spi_flash_info.dev, addr, data, size);
	if(ret){
		printk("%s: read from flash failed!\n",__func__);
		return -1;
	}

	return ret;
}

static u8 *panel_get_waveform_from_flash(int offset, u8 *buffer, int buffer_size)
{
	off_t wfm_base = DEFAULT_WFM_ADDR;

	if (panel_info_cache)
		wfm_base = panel_info_cache->addrs->waveform_addr;

	pr_debug("Reading waveform.. (%d bytes)\n", buffer_size);
	panel_read_from_flash(wfm_base + offset, buffer, buffer_size);
	return buffer;
}

static bool eink_waveform_valid(u8 *wf_buffer)
{
	struct eink_waveform_info_t *info;
	struct panel_info *panel_info;

	if (panel_info_cache && panel_info_cache->waveform_info) {
		info = panel_info_cache->waveform_info;
		panel_info = panel_info_cache;
	} else {
		printk(KERN_ERR "%s:panel waveform info not ready.\n", __func__);
		return false;
	}

	if (info->filesize <= WFM_HDR_SIZE || info->filesize > panel_info->addrs->waveform_len) {
		printk(KERN_ERR "eink_fb_waveform: E invalid:Invalid filesize in waveform header:\n");
		return false;
	}

	return true;
}

unsigned long eink_get_computed_waveform_checksum(u8 *wf_buffer)
{
	unsigned long checksum = 0;

	if (wf_buffer) {
		struct waveform_data_header *header = (struct waveform_data_header *)wf_buffer;
		unsigned long filesize = header->file_length;

		if (filesize) {
			unsigned long saved_embedded_checksum;

			// Save the buffer's embedded checksum and then set it zero.
			//
			saved_embedded_checksum = header->checksum;
			header->checksum = 0;

			// Compute the checkum over the entire buffer, including
			// the zeroed-out embedded checksum area, and then restore
			// the embedded checksum.
			//
			checksum = crc32((unsigned char *)wf_buffer, filesize);
			header->checksum = saved_embedded_checksum;
		} else {
			unsigned char checksum1, checksum2;
			int start, length;

			// Checksum bytes 0..(EINK_ADDR_CHECKSUM1 - 1).
			//
			start     = 0;
			length    = EINK_ADDR_CHECKSUM1;
			checksum1 = sum8((unsigned char *)wf_buffer + start, length);

			// Checksum bytes (EINK_ADDR_CHECKSUM1 + 1)..(EINK_ADDR_CHECKSUM2 - 1).
			//
			start     = EINK_ADDR_CHECKSUM1 + 1;
			length    = EINK_ADDR_CHECKSUM2 - start;
			checksum2 = sum8((unsigned char *)wf_buffer + start, length);

			checksum  = EINK_CHECKSUM(checksum1, checksum2);
		}
	}

	return checksum;
}

static u8 *panel_get_waveform(u8 *wf_buffer, size_t wf_buffer_len, bool header_only)
{
	if (!panel_flash_present()) {
		wf_buffer[0] = 0;
		return NULL;
	}

	epd_spi_printk("%s: reading waveform header\n", __FUNCTION__);

	if (wf_buffer_len < WFM_HDR_SIZE) {
		printk(KERN_ERR "%s: buffer not large enough (header)\n", __func__);
		return NULL;
	}

	if (panel_get_waveform_from_flash(0, wf_buffer, WFM_HDR_SIZE) == NULL) {
		printk(KERN_ERR "%s: Could not read header from flash\n", __func__);
		return NULL;
	}

	// We may end up having to re-read it if the initial read and/or subsequent
	// reads are invalid.
	//
	if (!header_only) {
		if (eink_waveform_valid(wf_buffer)) {
			eink_waveform_info_t *info;
			info = panel_info_cache->waveform_info;

			if (wf_buffer_len < info->filesize) {
				printk(KERN_ERR "Buffer not large enough (waveform)\n");
				return NULL;
			}

			if (panel_get_waveform_from_flash(WFM_HDR_SIZE,
						(wf_buffer + WFM_HDR_SIZE),
						(info->filesize - WFM_HDR_SIZE)) == NULL) {
				printk(KERN_ERR "%s: Could not read waveform from flash\n", __func__);
				return NULL;
			}

			epd_spi_printk("%s: verifying waveform checksum\n", __FUNCTION__);

			// Verify waveform checksum
			if (eink_get_computed_waveform_checksum(wf_buffer) != info->checksum) {
				printk(KERN_ERR "Invalid waveform checksum\n");
				return NULL;
			} else {
				panel_info_cache->computed_checksum = info->checksum;
			}

			epd_spi_printk("%s: read waveform size %ld\n", __FUNCTION__, info->filesize);
		} else {
			printk(KERN_ERR "%s,Invalid waveform header\n", __func__);
			return NULL;
		}
	}

	return wf_buffer;
}

char *panel_get_bcd(struct panel_info *panel)
{
        u8 bcd[PNL_SIZE_BCD] = { 0 };

	if (panel == NULL) {
		printk(KERN_ERR "%s: panel is NULL\n", __func__);
		return NULL;
	}

        if (!panel_flash_present()) {
                panel->bcd[0] = '\0';
                return panel->bcd;
        }

        panel_read_from_flash(panel->addrs->pnl_info_addr + PNL_BASE_BCD, bcd, PNL_SIZE_BCD);
        panel_data_translate(bcd, PNL_SIZE_BCD);
        strncpy(panel->bcd, bcd, PNL_SIZE_BCD);
        panel->bcd[PNL_SIZE_BCD] = '\0';

        epd_spi_printk("%s: panel bcd=%s\n", __FUNCTION__, panel->bcd);

        return panel->bcd;
}

char *panel_get_id(struct panel_info *panel)
{
	u8 panel_buffer[PNL_BASE_FPL + 4] = { 0 };
	char *part_number;
	int cur;

	if (panel == NULL) {
		printk(KERN_ERR "%s: panel is NULL\n", __func__);
		return NULL;
	}

	if (!panel_flash_present()) {
		panel->id[0] = '\0';
		return panel->id;
	}

	// Waveform file names are of the form PPPP_XLLL_DD_TTVVSS_B, and
	// panel IDs are of the form PPPP_LLL_DD_MMM.
	//
	panel_read_from_flash(panel->addrs->pnl_info_addr, panel_buffer, sizeof(panel_buffer));
	panel_data_translate(panel_buffer, sizeof(panel_buffer));

	// The platform is (usually) the PPPP substring.  And, in those cases, we copy
	// the platform data from the EEPROM's waveform name.  However, we must special-case
	// the V220E waveforms since EINK isn't using the same convention as they did in
	// the V110A case (i.e., they named V110A waveforms 110A but they are just
	// calling the V220E waveforms V220 with a run-type of E; run-type is the X
	// field in the PPPP_XLLL_DD_TTVVSS_B part of waveform file names).
	//
	switch (panel_buffer[PNL_BASE_WAVEFORM + 5]) {
		case 'e':
			panel->id[0] = '2';
			panel->id[1] = '2';
			panel->id[2] = '0';
			panel->id[3] = 'e';
			break;

		default:
			panel->id[0] = panel_buffer[PNL_BASE_WAVEFORM + 0];
			panel->id[1] = panel_buffer[PNL_BASE_WAVEFORM + 1];
			panel->id[2] = panel_buffer[PNL_BASE_WAVEFORM + 2];
			panel->id[3] = panel_buffer[PNL_BASE_WAVEFORM + 3];
			break;
	}

	panel->id[ 4] = '_';

	// the lot number (aka fpl) is the the lll substring:  just
	// copy the number itself, skipping the batch (x) designation.
	//
	panel->id[ 5] = panel_buffer[PNL_BASE_FPL + 1];
	panel->id[ 6] = panel_buffer[PNL_BASE_FPL + 2];
	panel->id[ 7] = panel_buffer[PNL_BASE_FPL + 3];

	panel->id[ 8] = '_';

	// the display size is the the dd substring.
	//
	panel->id[ 9] = panel_buffer[PNL_BASE_WAVEFORM + 10];
	panel->id[10] = panel_buffer[PNL_BASE_WAVEFORM + 11];
	panel->id[11] = '_';

	/* copy in the full part number */
	part_number = &panel_buffer[PNL_BASE_PART_NUMBER];
	for (cur = 0; cur < PNL_SIZE_PART_NUMBER && part_number[cur] != PNL_CHAR_UNKNOWN; cur++)
		panel->id[12 + cur] = part_number[cur];

	panel->id[12 + cur] = 0;

	if (!panel_data_valid(panel->id))
		strcpy(panel->id, PANEL_ID_UNKNOWN);

	epd_spi_printk("%s: panel id=%s\n", __FUNCTION__, panel->id);

	return panel->id;
}

char *eink_get_wfm_version(u8 *wf_buffer, char *version_string, size_t version_string_len)
{
	struct eink_waveform_info_t *info;
	// Build up a waveform version string in the following way:
	//
	//      <FPL PLATFORM>_<RUN TYPE>_<FPL LOT NUMBER>_<FPL SIZE>_
	//      <WF TYPE><WF VERSION><WF SUBVERSION>_
	//      (<WAVEFORM REV>|<TUNING BIAS>)_<MFG CODE>_<S/N>_<FRAME RATE>_MODEVERSION
	if (panel_info_cache && panel_info_cache->waveform_info) {
		info = panel_info_cache->waveform_info;
		snprintf(version_string,
			version_string_len,
			"%02x_%02x_%04x_%02x_%02x%02x%02x_%02x_%02x_%08x_%02x_%02x",
			info->fpl.platform,
			info->waveform.run_type,
			info->fpl.lot,
			info->fpl.size,
			info->waveform.type,
			info->waveform.version,
			info->waveform.subversion,
			(info->waveform.type == EINK_WAVEFORM_TYPE_WR) ? info->waveform.revision : info->waveform.tuning_bias,
			info->waveform.mfg_code,
			(unsigned int) info->waveform.serial_number,
			info->waveform.fpl_rate,
			info->waveform.mode_version);
	} else {
		version_string[0] = '\0';
	}

	epd_spi_printk("%s: waveform version=%s\n", __FUNCTION__, version_string);

	return version_string;
}

char *eink_get_pnl_wfm_human_version(u8 *wf_buffer, size_t wf_buffer_len, char *str, size_t str_len)
{
	struct eink_waveform_info_t *info;
	struct panel_info *panel_info;
	u8 buf[str_len];
	u8 len = str_len - 1;

	if (panel_info_cache && panel_info_cache->waveform_info) {
		info = panel_info_cache->waveform_info;
		panel_info = panel_info_cache;
	} else {
		printk(KERN_ERR "%s, panel waveform info not ready.\n", __func__);
		goto error;
	}

	epd_spi_printk("%s: reading embedded filename\n", __func__);

	/* Make sure there is a pointer to XWIA area */
	if (!info->waveform.xwia) {
		str[0] = '\0';
		return str;
	}

	// Check to see if the XWI is contained within the buffer
	if ((info->waveform.xwia >= wf_buffer_len) || ((wf_buffer[info->waveform.xwia] + info->waveform.xwia) >= wf_buffer_len)) {
		if (!panel_flash_present())
			goto error;

		if (panel_read_from_flash((panel_info->addrs->waveform_addr + info->waveform.xwia), buf, str_len)) {
			printk(KERN_ERR "Error reading from panel flash!\n");
			goto error;
		}

		// XWI[0] is the XWI length
		if (buf[0] < len)
			len = buf[0];

		memmove(str, buf + 1, len);
		str[len] = '\0';
	} else {
		if (wf_buffer[info->waveform.xwia] < len)
			len = wf_buffer[info->waveform.xwia];

		memmove(str, wf_buffer + info->waveform.xwia + 1, len);
		str[len] = '\0';
	}

	epd_spi_printk("%s: wfm human version=%s\n", __FUNCTION__, str);

	return str;

error:
	snprintf(str, str_len, "?????");
	return str;
}

static int parse_vcom_str(char *vcom_str, int vcom_str_len)
{
	int vcom = 0;
	int i = 0;
	int fct_ord = 0;
	bool dec_pnt_reached = false;

	for(i = 0; i < vcom_str_len; i++) {
		if ('.' == vcom_str[i]) {
			dec_pnt_reached = true;
			continue;
		}

		if ((vcom_str[i] >= '0') && (vcom_str[i] <= '9')) {
			vcom *= 10;
			vcom += (vcom_str[i] - '0');

			if (dec_pnt_reached)
				fct_ord++;
		}
	}

	// Normalize value to uV
	i = 6 - fct_ord; // log(1,000,000)/log(frac(vcom))
	for (; i > 0; vcom *= 10, i--);

	return vcom;
}

static char *panel_get_vcom_str(char *vcom_str, size_t vcom_str_len)
{
	struct panel_info *panel_info;
	u8 vcom[PNL_SIZE_VCOM] = { 0 };
	epd_spi_printk("%s begin\n", __FUNCTION__);

	if (!panel_flash_present() || panel_info_cache == NULL) {
		vcom_str = NULL;
		return vcom_str;
	}

	panel_info = panel_info_cache;

	if (panel_read_from_flash((panel_info->addrs->pnl_info_addr + PNL_BASE_VCOM), vcom, PNL_SIZE_VCOM)) {
		printk(KERN_ERR "Error reading from panel flash!\n");
		vcom_str = NULL;
		return vcom_str;
	}

	/* Decode panel data */
	panel_data_translate(vcom, sizeof(vcom));

	strncpy(vcom_str, vcom, min((size_t)PNL_SIZE_VCOM, vcom_str_len - 1));
	vcom_str[min((size_t)PNL_SIZE_VCOM, vcom_str_len - 1)] = '\0';


	// If the VCOM string returned from the panel data is invalid, then
	// use the default one instead.
	//

	if (!panel_data_valid(vcom_str)) {
		printk(KERN_ERR "Panel flash data is not valid!\n");
		vcom_str = NULL;
		return vcom_str;
	}

	epd_spi_printk("%s vcom=%s\n", __FUNCTION__, vcom_str);

	return vcom_str;
}

static int panel_get_vcom(struct panel_info *panel)
{
	char vcom_str[PNL_SIZE_VCOM_STR] = { 0 };

	if (panel == NULL) {
		printk("%s, panel info is not ready.\n", __func__);
		return 0;
	}

	if (panel_get_vcom_str(vcom_str, PNL_SIZE_VCOM_STR) == NULL) {
		printk(KERN_ERR "eink_fb_waveform: E vcom:Setting VCOM to default value -2.05V!!!\n");
		panel->vcom_uV = -2050000;
		return panel->vcom_uV;
	}

	// Skip the negative sign (i.e., i = 1, instead of i = 0).
	if ('-' == (char)vcom_str[0])
		panel->vcom_uV = -parse_vcom_str(vcom_str + 1, PNL_SIZE_VCOM - 1);
	else
		panel->vcom_uV = parse_vcom_str(vcom_str, PNL_SIZE_VCOM);

	epd_spi_printk("%s vcom=%dmV\n", __FUNCTION__, uV_to_mV(panel->vcom_uV));

	return panel->vcom_uV;
}

static int panel_get_info(struct panel_info **panel, bool header_only)
{
	u8 *panel_waveform = NULL;

	if (panel_info_cache == NULL) {
		panel_waveform = kmalloc(WFM_HDR_SIZE, GFP_KERNEL);

		if (panel_waveform == NULL) {
			printk(KERN_ERR "%s: allocate failed for header (%d bytes)", 
					__func__, WFM_HDR_SIZE);
			return -ENOMEM;
		}

		if (panel_get_waveform(panel_waveform, WFM_HDR_SIZE, true) == NULL) {
			printk(KERN_ERR "%s: could not read waveform", __func__);
			kfree(panel_waveform);
			return -ENXIO;
		}

		panel_info_cache = kmalloc(sizeof(struct panel_info), GFP_KERNEL);
		if (panel_info_cache == NULL) {
			printk(KERN_ERR "%s: allocate failed for panel_info_cache(%d bytes)",
					__func__, sizeof(struct panel_info));
			kfree(panel_waveform);
			return -ENOMEM;
		}

		panel_info_cache->waveform_info = kmalloc(sizeof(struct eink_waveform_info_t), GFP_KERNEL);
		if (panel_info_cache->waveform_info == NULL) {
			printk(KERN_ERR "%s: allocate failed for waveform_info (%d bytes)",
					__func__, sizeof(struct eink_waveform_info_t));
			kfree(panel_waveform);
			kfree(panel_info_cache);
			panel_info_cache = NULL;
			return -ENOMEM;
		}

		panel_get_format(panel_info_cache, (struct waveform_data_header *)panel_waveform);

		panel_get_bcd(panel_info_cache);
		panel_get_id(panel_info_cache);

		panel_info_cache->embedded_checksum = ((struct waveform_data_header *)panel_waveform)->checksum;
		panel_info_cache->computed_checksum = -1;

		eink_get_waveform_info(panel_waveform, panel_info_cache->waveform_info);
		eink_get_wfm_version(panel_waveform, panel_info_cache->version, WAVEFORM_VERSION_STRING_MAX);
		eink_get_pnl_wfm_human_version(panel_waveform, WFM_HDR_SIZE, panel_info_cache->human_version, WAVEFORM_VERSION_STRING_MAX);

		panel_get_vcom(panel_info_cache);

		kfree(panel_waveform);
		panel_waveform = NULL;
	}

	if (panel_info_cache->computed_checksum == -1 && !header_only) {
		panel_waveform = kmalloc(panel_info_cache->addrs->waveform_len, GFP_KERNEL);

		if (panel_waveform == NULL) {
			printk(KERN_ERR "%s: could not allocate space for waveform (%d bytes)", __func__, panel_info_cache->addrs->waveform_len);
			return -ENOMEM;
		}

		if (panel_get_waveform(panel_waveform, panel_info_cache->addrs->waveform_len, false) == NULL) {
			printk(KERN_ERR "%s: could not read waveform", __func__);
			kfree(panel_waveform);
			return -ENXIO;
		}

		//panel_info_cache->computed_checksum = eink_get_computed_waveform_checksum(panel_waveform);
		kfree(panel_waveform);
		panel_waveform = NULL;
	}

	*panel = panel_info_cache;

	return 0;
}

static bool check_spi_flash_id(void)
{
	u8 cmd;
	u8 mfg;
	u32 res;
	int ret;

	cmd = SFM_ID;
 	ret = spi_write_then_read(spi_flash_info.dev, &cmd, 1, &res, 3);
	if (ret) {
		printk(KERN_ERR "%s: spi_write_then_read err.\n", __func__);
	}

	mfg = res & 0xFF;
	switch (mfg) {
		case 0x20:
			printk("M25P20 flash detected\n");
			break;
		case 0xc0:
			printk("MX25L2006E flash detected\n"); /* 3.3V */
			break;
		case 0xc2:
			printk("MX25L2005 flash detected\n");
			break;
		case 0xef:
			printk("MX25U4035 flash detected\n");
			break;

		case 0x00:
			printk(KERN_ERR "Bad flash signature: 0x%x\n", res);
			spi_flash_info.dev = NULL;
			return false;

		default:
			printk("Unrecognized flash: 0x%x\n", mfg);
			break;
	}

	return true;
}

static int rockchip_set_vcom(struct panel_info *panel, int vcom_mV)
{
	int ret = 0;
	char mfg_code = 0;
	int mode_version = 0;
	struct eink_waveform_info_t *info;

	if (panel && panel->waveform_info) {
		info = panel->waveform_info;
	} else {
		printk(KERN_ERR "%s:panel waveform info not available.\n", __func__);
		return -1;
	}

	mode_version = info->waveform.mode_version;
	if (mode_version == WF_UPD_MODES_24 || mode_version == WF_UPD_MODES_25) {
		printk("Calculate vcom for XD4\n");
		if (info->waveform.type != EINK_WAVEFORM_TYPE_WR && info->waveform.vcom_shift)
		{
			printk("Vcom shift enabled. Shifting by: +%d mV\n", WAVEFORM_AA_VCOM_SHIFT);
			vcom_mV += WAVEFORM_AA_VCOM_SHIFT;
		}
	} else {
		printk("Calculate vcom for XC3\n");
		if (panel->bcd) {
			mfg_code = panel->bcd[BCD_MODULE_MFG_OFFSET];
		} else {
			printk(KERN_ERR "%s:panel bcd not available.\n", __func__);
			return -1;
		}

		if (mfg_code == 'A' || mfg_code == 'B' ||
				mfg_code == 'C' || mfg_code == 'D' ||
				mfg_code == 'Q' || mfg_code == 'U')
		{
			// We are using an old waveform on a new panel, need to add VCOM shift
			if (!info->waveform.vcom_shift) {
				vcom_mV += 400;
				printk("Vcom shifting by +400 mV\n");
			}

		} else {
			// We are using a -400mV tuned waveform on an old panel
			if (info->waveform.vcom_shift) {
				vcom_mV -= 400;
				printk("Vcom shifting by -400 mV\n");
			}
		}

	}

	ret = tps65185_vcom_set(vcom_mV);
	if (ret)
		printk("unable to set VCOM = %dmV (err = %d)\n", vcom_mV, ret);
	else
		printk("Final VCOM = %dmV\n", vcom_mV);

	return ret;
}

static void spi_flash_power_on(int on)
{
	struct regulator *ldo_spi1v8 = NULL;
	struct regulator *ldo_spi3v3 = NULL;
	int ret = 0;

	ldo_spi1v8 = regulator_get(NULL, "dldo3");
	if (!ldo_spi1v8) {
		printk("spi 1.8V power not available.\n");
		return; 
	}
	ldo_spi3v3 = regulator_get(NULL, "axp22_eldo3");
	if (!ldo_spi3v3) {
		printk("spi 3.3V power not available.\n");
		return;
	}
	if(on) {
		if (!regulator_is_enabled(ldo_spi1v8)) {
			regulator_enable(ldo_spi1v8);
			printk("%s: spi 1.8V power enable\n", __func__);
		}
		if (!regulator_is_enabled(ldo_spi3v3)) {
			regulator_enable(ldo_spi3v3);
			printk("%s: spi 3.3V power enable\n", __func__);
		}
	} else {
		while(regulator_is_enabled(ldo_spi1v8)>0) {
			regulator_disable(ldo_spi1v8);
			printk("%s: spi 1.8V power disable\n", __func__);
		}
		while(regulator_is_enabled(ldo_spi3v3)>0) {
			regulator_disable(ldo_spi3v3);
			printk("%s: spi 3.3V power disable\n", __func__);
		}
	}
	regulator_put(ldo_spi1v8);
	regulator_put(ldo_spi3v3);
	msleep(40);

	/* set spi io */
	if (!on) {
		ret = gpio_request(RK30_PIN1_PB0, "gpio_spi_clk");
		if (ret) {
			printk("%s:Request GPIO %d failed\n", __func__, RK30_PIN1_PB0);
		} else {
			gpio_pull_updown(RK30_PIN1_PB0, PullDisable);
			gpio_direction_input(RK30_PIN1_PB0);
		}

		ret = gpio_request(RK30_PIN1_PB1, "gpio_spi_mosi");
		if (ret) {
			printk("%s:Request GPIO %d failed\n", __func__, RK30_PIN1_PB1);
		} else {
			gpio_pull_updown(RK30_PIN1_PB1, PullDisable);
			gpio_direction_input(RK30_PIN1_PB1);
		}

		ret = gpio_request(RK30_PIN1_PB2, "gpio_spi_miso");
		if (ret) {
			printk("%s:Request GPIO %d failed\n", __func__, RK30_PIN1_PB2);
		} else {
			gpio_pull_updown(RK30_PIN1_PB2, PullDisable);
			gpio_direction_input(RK30_PIN1_PB2);
		}

		ret = gpio_request(RK30_PIN1_PB3, "gpio_spi_cs");
		if (ret) {
			printk("%s:Request GPIO %d failed\n", __func__, RK30_PIN1_PB3);
		} else {
			gpio_pull_updown(RK30_PIN1_PB3, PullDisable);
			gpio_direction_input(RK30_PIN1_PB3);
		}
	}
}
#endif

static ssize_t spi_flash_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	u8* kbuf = NULL;
#if defined(CONFIG_PVI_WAVEFORM)
	struct panel_info *panel_info;
	unsigned long kbuf_len;
	int vcom_mv;
	int i;
	int ret;

	epd_spi_printk("enter spi flash read.\n");

	spi_flash_power_on(1);

	/* we should check id in probe, but the power not ready at that time */
	check_spi_flash_id();

	if (!panel_flash_present()) {
		return -1;
	}

	if ((ret = panel_get_info(&panel_info, true))) {
		printk(KERN_ERR "%s: get panel info failed (%d)\n", __func__, ret);
		return ret;
	}

	if (panel_info && panel_info->waveform_info) {
		//kbuf_len = panel_info_cache->addrs->waveform_len;
		kbuf_len = panel_info_cache->waveform_info->filesize;
		kbuf = (u8*)kmalloc(kbuf_len, GFP_KERNEL);

		if (kbuf_len > EINK_WAVEFORM_FILESIZE) {
			printk("[wfm size bigger then rk defined!(%ld)]\n", kbuf_len);
		}

		if (kbuf == NULL) {
			printk(KERN_ERR "%s: could not allocate space for waveform (%ld bytes)", __func__, kbuf_len);
			return -ENOMEM;
		}

		if (panel_get_waveform(kbuf, kbuf_len, false) == NULL) {
			printk(KERN_ERR "%s: could not read waveform", __func__);
			kfree(kbuf);
			return -ENXIO;
		}

		printk("waveform file size: %ld\n", kbuf_len);

		vcom_mv = uV_to_mV(panel_info->vcom_uV);
		printk("vcom from spi flash: %dmV\n", vcom_mv);
		rockchip_set_vcom(panel_info, vcom_mv);
	}

	spi_flash_power_on(0);

	buf = panel_info_cache->id;
	for(i=0;i<PNL_SIZE_ID_STR;i++)
		kbuf[EINK_WAVEFORM_FILESIZE+i]=buf[i];
	return (int)kbuf;
#else
	u32 addr = 0;
	if(SPIFlashRead(spi_flash_info.dev, addr ,kbuf , count))
	{
		kfree(kbuf);
		return -1;
	}
	
	else
	{
		if(copy_to_user(buf, (char *)kbuf, count))
		{
			kfree(kbuf);
			return -EFAULT;
		}
		return count;
	}
#endif
}

static int spi_flash_open(struct inode *inode, struct file *file)
{
	epd_spi_printk("enter spi flash open.\n");
	file->f_pos = 0;
	return 0;
}
static ssize_t spi_flash_write(struct file *file, const char __user *data,
			      size_t len, loff_t * ppos)
{
	char *write_data=NULL;
	u32 addr = 0;

	if(ppos)
		addr = *ppos;   
	write_data = (char*)kmalloc(len, GFP_KERNEL);

	if(copy_from_user(write_data, data, len))
	{
		kfree(write_data);
		return -EFAULT;
	}
	spi_write_data(spi_flash_info.dev,addr,(char*)write_data,len);

	kfree(write_data);
	return len;

}

/* seek文件定位函数 */
static loff_t spi_flash_seek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret = 0;
	
	switch (orig)
	{
		case 0:   /*相对文件开始位置偏移*/
			if (offset < 0)
			{
				ret =  - EINVAL;
				break;
			}
			if ((unsigned int)offset > FLASH_GLOBAl_SIZE)
			{
				ret =  - EINVAL;
				break;
			}
			filp->f_pos = (unsigned int)offset;
			ret = filp->f_pos;
			break;
			
		case 1:   /*相对文件当前位置偏移*/
			if ((filp->f_pos + offset) > FLASH_GLOBAl_SIZE)
			{
				ret =  - EINVAL;
				break;
			}
			if ((filp->f_pos + offset) < 0)
			{
				ret =  - EINVAL;
				break;
			}
			filp->f_pos += offset;
			ret = filp->f_pos;
			break;
			
		default:
			ret =  - EINVAL;
			break;
	}
	
	return ret;
}

static struct file_operations spi_flash_fops = {
	.owner	= THIS_MODULE,
	.open   = spi_flash_open,
	.write  = spi_flash_write,
	.read   = spi_flash_read,
	.llseek = spi_flash_seek
};

static int  __devinit spi_flash_probe(struct spi_device *spi)
{        
	short flash_major = 0;
	dev_t devno; 
	int err =0;

	epd_spi_printk("enter spi flash probe.\n");

	spi_flash_info.dev = spi;

	//  register_chrdev(0, "spi_flash", &spi_flash_fops);
	/* create your own class under /sysfs */
	 devno = MKDEV(flash_major, 0);
	 if(flash_major)
	 {
		register_chrdev_region(devno, 1, "spi_flash");
	 }
	 else
	 {
		alloc_chrdev_region(&devno,0, 1, "spi_flash");
		flash_major = MAJOR(devno);
	 }

	spi_flash_info.cdev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if(NULL == spi_flash_info.cdev)
	{
		epd_spi_printk("no mem.\n");
		return ENOMEM;
	}
	cdev_init(spi_flash_info.cdev, &spi_flash_fops);
	spi_flash_info.cdev->owner = THIS_MODULE;
	spi_flash_info.cdev->ops = &spi_flash_fops;
	err = cdev_add(spi_flash_info.cdev, devno, 1);
	if(err)
		epd_spi_printk("adding spi flash error.\n");

	spi_flash_info.my_class = class_create(THIS_MODULE, "spi_flash");
	if(IS_ERR(spi_flash_info.my_class)) 
	{
		epd_spi_printk("Err: failed in creating spi flash class.\n");
		return -1; 
	} 
	device_create(spi_flash_info.my_class, NULL, devno,NULL, "spi_flash"); 

	epd_spi_flash_register(&spi_flash_info);

	epd_spi_printk("spi flash probe ok.\n");
		
	return 0;
}

static  int __devexit spi_flash_remove(struct spi_device *pdev)
{  
	epd_spi_printk("enter spi flash remove.\n");

	if(spi_flash_info.cdev)
	{
		cdev_del(spi_flash_info.cdev);
		kfree(spi_flash_info.cdev);
	}
	if(spi_flash_info.my_class)
	{
		device_destroy(spi_flash_info.my_class, 0);         //delete device node under /dev
		class_destroy(spi_flash_info.my_class);               //delete class created by us
	}

	return 0;
}

#ifdef CONFIG_PM
int spi_flash_suspend(struct spi_device *spi, pm_message_t state)
{
    epd_spi_printk("suspend.\n");
     
    return 0;
}

int spi_flash_resume(struct spi_device *spi)
{
    epd_spi_printk("resume.\n");
  
    return 0;
}
#endif 

static struct spi_driver spi_flash_driver = 
{
	.driver = {
		.name = "epd_spi_flash",
		.bus	  = &spi_bus_type,
		.owner = THIS_MODULE,
	},
	.probe = spi_flash_probe,
	.remove = __devexit_p(spi_flash_remove),
#ifdef CONFIG_PM
	.suspend = spi_flash_suspend,
	.resume = spi_flash_resume
#endif
};

static int __init spi_flash_init(void)
{
	int ret = spi_register_driver(&spi_flash_driver);

	if (ret) {
		printk(KERN_ERR "spi driver registration failed: %d\n", ret);
		spi_registered = false;
	} else {
		spi_registered = true;
	}

	return ret;
}

static void __exit spi_flash_exit(void)
{
	printk("%s: enter...\n", __func__);

	if (spi_registered) {
		spi_unregister_driver(&spi_flash_driver);
		spi_registered = false;
	}
}

subsys_initcall(spi_flash_init);
module_exit(spi_flash_exit);
MODULE_AUTHOR("dlx@rock-chips.com");
MODULE_DESCRIPTION("rockchip rk29 extern spi flash");
MODULE_LICENSE("GPL");
