#ifndef _EINK_WAVEFORM_H
#define _EINK_WAVEFORM_H

#define EINK_WAVEFORM_FILESIZE          262144  // 256K..
#define EINK_WAVEFORM_COUNT             0       // ..max

#define EINK_WAVEFORM_PROXY_SIZE        (1024 * 1024 * 2)

#define EINK_ADDR_CHECKSUM1             0x001F  // 1 byte  (checksum of bytes 0x00-0x1E)
#define EINK_ADDR_CHECKSUM2             0x002F  // 1 byte  (checksum of bytes 0x20-0x2E)
#define EINK_ADDR_CHECKSUM              0x0000  // 4 bytes (was EINK_ADDR_MFG_DATA_DEVICE)
#define EINK_ADDR_FILESIZE              0x0004  // 4 bytes (was EINK_ADDR_MFG_DATA_DISPLAY)
#define EINK_ADDR_MFG_CODE              0x0015  // 1 byte  (0x00..0xFF -> M00..MFF)
#define EINK_ADDR_SERIAL_NUMBER         0x0008  // 4 bytes (little-endian)

#define EINK_ADDR_RUN_TYPE              0x000C  // 1 byte  (0x00=[B]aseline, 0x01=[T]est/trial, 0x02=[P]roduction, 0x03=[Q]ualification, 0x04=V110[A],
                                                //          0x05=V220[C], 0x06=D, 0x07=V220[E], 0x08-0x10=F-N)

#define EINK_ADDR_FPL_PLATFORM          0x000D  // 1 byte  (0x00=2.0, 0x01=2.1, 0x02=2.3; 0x03=V110, 0x04=V110A, 0x06=V220, 0x07=V250, 0x08=V220E)
#define EINK_ADDR_FPL_SIZE              0x0014  // 1 byte  (0x32=5", 0x3C=6", 0x3F=6" 0x50=8", 0x61=9.7", 0x63=9.7")
#define EINK_ADDR_FPL_LOT               0x000E  // 2 bytes (little-endian)
#define EINK_ADDR_ADHESIVE_RUN_NUM      0x0010  // 1 byte  (mode version when EINK_ADDR_FPL_PLATFORM is 0x03 or later)
#define EINK_ADDR_MODE_VERSION          0x0010  // 1 byte  (0x00=MU/GU/GC/PU, 0x01,0x02=DU/GC16/GC4, 0x03=DU/GC16/GC4/AU, 0x04=DU/GC16/AU)

#define EINK_ADDR_WAVEFORM_VERSION      0x0011  // 1 byte  (BCD)
#define EINK_ADDR_WAVEFORM_SUBVERSION   0x0012  // 1 byte  (BCD)
#define EINK_ADDR_WAVEFORM_TYPE         0x0013  // 1 byte  (0x0B=TE, 0x0E=WE, 0x15=WJ, 0x16=WK, 0x17=WL, 0x18=VJ, 0x2B=WR)
#define EINK_ADDR_WAVEFORM_TUNING_BIAS  0x0016  // 1 byte  (WJ-type and earlier) (0x00=Standard, 0x01=Increased DS Blooming V110/V110E, 0x02=Increased DS Blooming V220/V220E,
                                                //          0x03=0x02=Increased DS Blooming V220/V220E & Improved Temperature Range V220/V220E)
#define EINK_ADDR_WAVEFORM_REV		0x0016  // 1 byte  (WR-type and later) Waveform revision
#define EINK_ADDR_FPL_RATE              0x0017  // 1 byte  (0x50=50Hz, 0x60=60Hz, 0x85=85Hz)
#define EINK_ADDR_VCOM_SHIFT            0x0019  // 1 byte  non-zero if VCOM was shifted to compensate for edge ghosting
#define EINK_ADDR_XWIA			0x001C  // 3 bytes  Extra Waveform Information address

// PVI/EIH panel values
#define EINK_MFG_CODE_EIH_E40           0x33    // ED060SCF (V220 6” Tequila)
#define EINK_MFG_CODE_EIH_E48           0x34    // ED060SCFH1 (V220 Tequila Hydis – Line 2)
#define EINK_MFG_CODE_EIH_E49           0x35    // ED060SCFH1 (V220 Tequila Hydis – Line 3)
#define EINK_MFG_CODE_EIH_E4A           0x36    // ED060SCFC1 (V220 Tequila CMO)
#define EINK_MFG_CODE_EIH_E4B           0x37    // ED060SCFT1 (V220 Tequila CPT)
#define EINK_MFG_CODE_EIH_E45           0x38    // ED060SCG (V220 Whitney)
#define EINK_MFG_CODE_EIH_E4E           0x39    // ED060SCGH1 (V220 Whitney Hydis – Line 2)
#define EINK_MFG_CODE_EIH_E4F           0x3A    // ED060SCGH1 (V220 Whitney Hydis – Line 3)
#define EINK_MFG_CODE_EIH_E4C           0x3B    // ED060SCGC1 (V220 Whitney CMO)
#define EINK_MFG_CODE_EIH_E4D           0x3C    // ED060SCGT1 (V220 Whitney CPT)

// LGD panel values
#define EINK_MFG_CODE_LGD_L00           0xA0
#define EINK_MFG_CODE_LGD_L01           0xA1
#define EINK_MFG_CODE_LGD_L02           0xA2
#define EINK_MFG_CODE_LGD_L03           0xA3    // LB060S03-RD02 (LGD Tequila Line 1)
#define EINK_MFG_CODE_LGD_L04           0xA4    // Reserved for 2nd LGD Tequila Line
#define EINK_MFG_CODE_LGD_L05           0xA5    // LB060S05-RD02 (LGD Whitney Line 1)
#define EINK_MFG_CODE_LGD_L06           0xA6    // Reserved for 2nd LGD Whitney Line
#define EINK_MFG_CODE_LGD_L07           0xA7
#define EINK_MFG_CODE_LGD_L08           0xA8

#define EINK_WAVEFORM_TYPE_LGD_VJ       0x18
#define EINK_WAVEFORM_TYPE_WR		0x2B

#define EINK_FPL_SIZE_60                0x3C    // 6.0-inch panel,  800 x  600
#define EINK_FPL_SIZE_63                0x3F    // 6.0-inch panel,  800 x  600
#define EINK_FPL_SIZE_61                0x3D    // 6.1-inch panel, 1024 x  768
#define EINK_FPL_SIZE_97                0x61    // 9.7-inch panel, 1200 x  825
#define EINK_FPL_SIZE_99                0x63    // 9.7-inch panel, 1600 x 1200

#define EINK_FPL_RATE_50                0x50    // 50Hz waveform
#define EINK_FPL_RATE_60                0x60    // 60Hz waveform
#define EINK_FPL_RATE_85                0x85    // 85Hz waveform

#define EINK_IMPROVED_TEMP_RANGE        0x03    // Don't clip the temperature if we see this value in EINK_ADDR_WAVEFORM_TUNING_BIAS.

struct waveform_data_header {
	unsigned int checksum:32;		/* 0x0000 */
	unsigned int file_length:32;		/* 0x0004 */
	unsigned int serial_number:32;		/* 0x0008 */
	unsigned int run_type:8;		/* 0x000C */
	unsigned int fpl_platform:8;		/* 0x000D */
	unsigned int fpl_lot:16;		/* 0x000E */
	unsigned int mode_version:8;		/* 0x0010 */
	unsigned int wf_version:8;		/* 0x0011 */
	unsigned int wf_subversion:8;		/* 0x0012 */
	unsigned int wf_type:8;			/* 0x0013 */
	unsigned int panel_size:8;		/* 0x0014 */
	unsigned int amepd_part_number:8; 	/* 0x0015 mfg code */
	unsigned int wf_revision:8;		/* 0x0016 */
	unsigned int frame_rate:8;		/* 0x0017 */
	unsigned int reserved1_0:8;
	unsigned int vcom_shifted:8;		/* 0x0019 */
	unsigned int reserved1_1:16;
	unsigned int xwia:24;			/* 0x001C */
	unsigned int cs1:8;			/* 0x001F */

	unsigned int wmta:24;			/* 0x0020 */
	unsigned int fvsn:8;			/* 0x0023 */
	unsigned int luts:8;			/* 0x0024 */
	unsigned int mc:8;			/* 0x0025 */
	unsigned int trc:8;			/* 0x0026 */
	unsigned int advanced_wfm_flags:8;	/* 0x0027 */
	unsigned int eb:8;			/* 0x0028 */
	unsigned int sb:8;			/* 0x0029 */
	unsigned int reserved0_1:8;
	unsigned int reserved0_2:8;
	unsigned int reserved0_3:8;
	unsigned int reserved0_4:8;
	unsigned int reserved0_5:8;
	unsigned int cs2:8;			/* 0x002F */
};

struct update_mode {
	unsigned char mode;
	char *name;
};

struct update_modes {
	struct update_mode init;
	struct update_mode du;
	struct update_mode gc16;
	struct update_mode gcf;
	struct update_mode gl16;
	struct update_mode glf;
	struct update_mode a2;
	struct update_mode du4;
	struct update_mode glr;
	struct update_mode glrd;
};

struct panel_addrs {
	off_t cmd_sec_addr;
	off_t waveform_addr;
	off_t pnl_info_addr;
	off_t test_sec_addr;
	size_t cmd_sec_len;
	size_t waveform_len;
	size_t pnl_info_len;
	size_t test_sec_len;
	size_t flash_end;
};

struct panel_attrs {
	struct update_modes update_modes;
	struct panel_addrs addrs;
};

struct panel_attrs panel_mode_07 = {
	.update_modes = {
		.init = { .mode = 0, .name = "init" },
		.du   = { .mode = 1, .name = "du" },
		.gc16 = { .mode = 2, .name = "gc16" },
		.gcf  = { .mode = 3, .name = "gc16_fast" },
		.gl16 = { .mode = 5, .name = "gl16" },
		.glf  = { .mode = 6, .name = "gl16_fast" },
		.a2   = { .mode = 4, .name = "a2" },
		.du4  = { .mode = 2, .name = "gc16" },
		.glr  = { .mode = 6, .name = "gl16_fast" },
		.glrd = { .mode = 3, .name = "gc16_fast" }
	},
	.addrs = {
		.cmd_sec_addr  = 0x00000,
		.waveform_addr = 0x00886,
		.pnl_info_addr = 0x30000,
		.test_sec_addr = 0x3E000,
		.cmd_sec_len   = 0x00886 - 0x00000,
		.waveform_len  = 0x30000 - 0x00886,
		.pnl_info_len  = 0x3E000 - 0x30000,
		.test_sec_len  = 0x40000 - 0x3E000,
		.flash_end     = 0x40000
	}
};

struct panel_attrs panel_mode_18 = {
	.update_modes = {
		.init = { .mode = 0, .name = "init" },
		.du   = { .mode = 1, .name = "du" },
		.gc16 = { .mode = 2, .name = "gc16" },
		.gcf  = { .mode = 3, .name = "gc16_fast" },
		.gl16 = { .mode = 5, .name = "gl16" },
		.glf  = { .mode = 6, .name = "gl16_fast" },
		.a2   = { .mode = 4, .name = "a2" },
		.du4  = { .mode = 7, .name = "du4" },
		.glr  = { .mode = 6, .name = "gl16_fast" },
		.glrd = { .mode = 3, .name = "gc16_fast" }
	},
	.addrs = {
		.cmd_sec_addr  = 0x00000,
		.waveform_addr = 0x00886,
		.pnl_info_addr = 0x30000,
		.test_sec_addr = 0x3E000,
		.cmd_sec_len   = 0x00886 - 0x00000,
		.waveform_len  = 0x30000 - 0x00886,
		.pnl_info_len  = 0x3E000 - 0x30000,
		.test_sec_len  = 0x40000 - 0x3E000,
		.flash_end     = 0x40000
	}
};

struct panel_attrs panel_mode_19 = {
	.update_modes = {
		.init = { .mode = 0, .name = "init" },
		.du   = { .mode = 1, .name = "du" },
		.gc16 = { .mode = 2, .name = "gc16_fast" },
		.gcf  = { .mode = 2, .name = "gc16_fast" },
		.gl16 = { .mode = 3, .name = "gl16_fast" },
		.glf  = { .mode = 3, .name = "gl16_fast" },
		.a2   = { .mode = 4, .name = "a2" },
		.du4  = { .mode = 2, .name = "gc16_fast" },
		.glr  = { .mode = 3, .name = "gl16_fast" },
		.glrd = { .mode = 2, .name = "gc16_fast" }
	},
	.addrs = {
		.cmd_sec_addr  = 0x00000,
		.waveform_addr = 0x00886,
		.pnl_info_addr = 0x30000,
		.test_sec_addr = 0x3E000,
		.cmd_sec_len   = 0x00886 - 0x00000,
		.waveform_len  = 0x30000 - 0x00886,
		.pnl_info_len  = 0x3E000 - 0x30000,
		.test_sec_len  = 0x40000 - 0x3E000,
		.flash_end     = 0x40000
	}
};

struct panel_attrs panel_mode_24 = {
	.update_modes = {
		.init = { .mode = 0, .name = "init" },
		.du   = { .mode = 1, .name = "du" },
		.gc16 = { .mode = 2, .name = "gc16_fast" },
		.gcf  = { .mode = 2, .name = "gc16_fast" },
		.gl16 = { .mode = 3, .name = "gl16_fast" },
		.glf  = { .mode = 3, .name = "gl16_fast" },
		.a2   = { .mode = 6, .name = "a2" },
		.du4  = { .mode = 2, .name = "gc16_fast" },
		.glr  = { .mode = 4, .name = "reagl" },
		.glrd = { .mode = 5, .name = "reagld" }
	},
	.addrs = {
		.cmd_sec_addr  = 0x00000,
		.waveform_addr = 0x00886,
		.pnl_info_addr = 0x70000,
		.test_sec_addr = 0x7E000,
		.cmd_sec_len   = 0x00886 - 0x00000,
		.waveform_len  = 0x70000 - 0x00886,
		.pnl_info_len  = 0x7E000 - 0x70000,
		.test_sec_len  = 0x80000 - 0x7E000,
		.flash_end     = 0x80000
	}
};

struct panel_attrs panel_mode_25 = {
	.update_modes = {
		.init = { .mode = 0, .name = "init" },
		.du   = { .mode = 1, .name = "du" },
		.gc16 = { .mode = 2, .name = "gc16_fast" },
		.gcf  = { .mode = 2, .name = "gc16_fast" },
		.gl16 = { .mode = 3, .name = "gl16_fast" },
		.glf  = { .mode = 3, .name = "gl16_fast" },
		.a2   = { .mode = 6, .name = "a2" },
		.du4  = { .mode = 7, .name = "du4" },
		.glr  = { .mode = 4, .name = "reagl" },
		.glrd = { .mode = 5, .name = "reagld" }
	},
	.addrs = {
		.cmd_sec_addr  = 0x00000,
		.waveform_addr = 0x00886,
		.pnl_info_addr = 0x70000,
		.test_sec_addr = 0x7E000,
		.cmd_sec_len   = 0x00886 - 0x00000,
		.waveform_len  = 0x70000 - 0x00886,
		.pnl_info_len  = 0x7E000 - 0x70000,
		.test_sec_len  = 0x80000 - 0x7E000,
		.flash_end     = 0x80000
	}
};

#endif // _EINK_WAVEFORM_H

