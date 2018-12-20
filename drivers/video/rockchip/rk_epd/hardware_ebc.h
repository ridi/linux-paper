#ifndef __HARDWARE_EBC_REG_H__
#define __HARDWARE_EBC_REG_H__

/* ebc_dsp_start */
#define m_FRAME_NUM			(0x3f << 2)
#define m_FRAME_START			(1)

#define v_FRAME_NUM(x)			(((x) & 0x3f) << 2)
#define v_FRAME_START(x)		(((x) & 1) << 0)

/* ebc_dsp_ctrl */
#define m_DISPLAY_SWAP			(3 << 30)
#define m_DISPLAY_UPDATE_MODE		(1 << 29)
#define m_DISPLAY_MODE			(1 << 28)
#define m_VCOM_MODE			(1 << 27)
#define m_SCLK_DIVIDE_RATE		(0xf << 16)
#define m_SDO_VALUE			(0xffff << 0)

#define v_DISPLAY_SWAP(x)		(((x) & 3) << 30)
#define v_DISPLAY_UPDATE_MODE(x)	(((x) & 1) << 29)
#define v_DISPLAY_MODE(x)		(((x) & 1) << 28)
#define v_VCOM_MODE(x)			(((x) & 1) << 27)
#define v_SCLK_DIVIDE_RATE(x)		(((x) & 0xf) << 16)
#define v_SDO_VALUE(x)			((x) & 0xffff < <0)

/* ebc_epd_ctrl */
#define m_EPD_PANEL			(1 << 5)
#define m_POWER_ENABLE			(0x07 << 2)
#define m_GATE_SCAN_DIRET		(1 << 1)
#define m_SOURCE_SCAN_DIRET		(1 << 0)

#define v_EPD_PANEL(x)			(((x) & 1) << 5)
#define v_POWER_ENABLE(x)		(((x) & 0x07) << 2)
#define v_GATE_SCAN_DIRET(x)		(((x) & 1) << 1)
#define v_SOURCE_SCAN_DIRET(x)		(((x) & 1) << 0)

/* ebc_win_ctrl */
#define m_WINENABLE			(1 << 17)
#define m_AHB_MASTER_INCR		(0x1f << 12)
#define m_AHB_MASTER_BURST_TYPE		(0x07 << 9)
#define m_WIN_FIFO			(0x7f << 2)
#define m_WIN_FMT			(0x03 << 0)

#define v_WINENABLE(x)			(((x) & 1) << 16)
#define v_AHB_MASTER_INCR(x)		(((x) & 0x1f) << 11)
#define v_AHB_MASTER_BURST_TYPE(x)	(((x) & 0x07) << 8)
#define v_WIN_FIFO(x)			(((x) & 0x3f) << 2)
#define v_WIN_FMT(x)			(((x) & 0x03) << 0)

/* ebc_int_status */
#define m_FRAMEEND_CLEAR		(1<<6)
#define m_DISPLAYEND_CLEAR		(1<<7)
#define m_FRAMEFLAG_CLEAR		(1<<8)
#define m_DISPLAYEND_MASK		(1<<4)

#define v_FRAMEEND_CLEAR(x)		(((x) & 1) << 6)
#define v_DISPLAYEND_CLEAR(x)		(((x) & 1) << 7)
#define v_FRAMEFLAG_CLEAR(x)		(((x) & 1) << 8)
#define v_DISPLAYEND_MASK(x)		(((x) & 1) << 4)

/* Registers operation methods declared */
#define WriteReg32(addr, data)		(*(volatile unsigned int *)(addr) = data)
#define EbcReadBit(inf, addr, msk)	((inf->regbak.addr = inf->preg->addr) & (msk))
#define EbcWrReg(inf, addr, val)	(inf->preg->addr = inf->regbak.addr = (val))
#define EbcRdReg(inf, addr)             (inf->preg->addr)
#define EbcSetBit(inf, addr, msk)	(inf->preg->addr = ((inf->regbak.addr) |= (msk)))
#define EbcClrBit(inf, addr, msk)       (inf->preg->addr = ((inf->regbak.addr) &= ~(msk)))
#define EbcSetRegBit(inf, addr, msk)    (inf->preg->addr = ((inf->preg->addr) |= (msk)))
#define EbcMskReg(inf, addr, msk, val)  (inf->regbak.addr) &= ~(msk); inf->preg->addr = (inf->regbak.addr |= (val))

typedef volatile struct tagEBC_REG
{
    unsigned int EBC_DSP_START;		// 0x00 Frame statrt register
    unsigned int EBC_EPD_CTRL;		// 0x04 EPD control register
    unsigned int EBC_DSP_CTRL;		// 0x08 Display control register
    unsigned int EBC_DSP_HTIMING0;	// 0x0c H-Timing setting register0
    unsigned int EBC_DSP_HTIMING1;	// 0x10 H-Timing setting register1
    unsigned int EBC_DSP_VTIMING0;	// 0x14 V-Timing setting register0
    unsigned int EBC_DSP_VTIMING1;      // 0x18 V-Timing setting register1
    unsigned int EBC_DSP_ACT_INFO;	// 0x1c ACTIVE width/height
    unsigned int EBC_WIN_CTRL;		// 0x20 Window ctrl
    unsigned int EBC_WIN_MST0;          // 0x24 Current win memory start
    unsigned int EBC_WIN_MST1;          // 0x28 Next win memory start
    unsigned int EBC_WIN_VIR;		// 0x2c Window vir width/height
    unsigned int EBC_WIN_ACT;           // 0x30 Window act width/height
    unsigned int EBC_WIN_DSP;		// 0x34 Window dsp width/height
    unsigned int EBC_WIN_DSP_ST;	// 0x38 Window display start piont
    unsigned int EBC_INT;               // 0x3c Interrupt register
    unsigned int EBC_VCOM0;		// 0x40 VCOM setting register0
    unsigned int EBC_VCOM1;             // 0x44 VCOM setting register1
    unsigned int EBC_VCOM2;             // 0x48 VCOM setting register2
    unsigned int EBC_VCOM3;             // 0x4c VCOM setting register3
    unsigned int EBC_CONFIG_DONE;       // 0X50 Config done register
} EBC_REG, *pEBC_REG;

#endif	// __HARDWARE_EBC_REG_H__
