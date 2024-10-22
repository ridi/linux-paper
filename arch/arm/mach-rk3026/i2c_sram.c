#include <linux/io.h>
#include <mach/board.h>
#include <mach/sram.h>
#include <mach/iomux.h>
#include <mach/cru.h>
#include <asm/io.h>
#include <mach/gpio.h>

#define cru_readl(offset)	readl_relaxed(RK2928_CRU_BASE + offset)
#define cru_writel(v, offset)	do { writel_relaxed(v, RK2928_CRU_BASE + offset); dsb(); } while (0)

#if defined(CONFIG_RK30_I2C_INSRAM)

/******************need set when you use i2c*************************/
#define I2C_SPEED 100
#define I2C_SADDR               (0x2D)        /* slave address ,wm8310 addr is 0x34*/
#define SRAM_I2C_CH 1	//CH==0, i2c0,CH==1, i2c1,CH==2, i2c2,CH==3, i2c3
#if defined (CONFIG_MACH_RK2928_SDK)|| defined( CONFIG_ARCH_RK3026_TB)||defined(CONFIG_ARCH_RK3028A_TB)
#define SRAM_I2C_ADDRBASE (RK2928_RKI2C1_BASE )//RK29_I2C0_BASE\RK29_I2C2_BASE\RK29_I2C3_BASE
#else
#define SRAM_I2C_ADDRBASE (RK2928_RKI2C0_BASE )
#endif

#define I2C_SLAVE_ADDR_LEN 1  // 2:slav addr is 10bit ,1:slav addr is 7bit
#define I2C_SLAVE_REG_LEN 1   // 2:slav reg addr is 16 bit ,1:is 8 bit
#define SRAM_I2C_DATA_BYTE 1  //i2c transmission data is 1bit(8wei) or 2bit(16wei)
#define GRF_GPIO_IOMUX 0xd4 //GRF_GPIO2D_IOMUX
/*ch=0:GRF_GPIO2L_IOMUX,ch=1:GRF_GPIO1L_IOMUX,ch=2:GRF_GPIO5H_IOMUX,ch=3:GRF_GPIO2L_IOMUX*/
#define I2C_GRF_GPIO_IOMUX (0x01<<14)|(0x01<<12)
/*CH=0:(~(0x03<<30))&(~(0x03<<28))|(0x01<<30)|(0x01<<28),CH=1:(~(0x03<<14))&(~(0x03<<12))|(0x01<<14)|(0x01<<12),
CH=2:(~(0x03<<24))&(~(0x03<<22))|(0x01<<24)|(0x01<<22),CH=3:(~(0x03<<26))&(~(0x03<<24))|(0x02<<26)|(0x02<<24)*/
/***************************************/

#define I2C_SLAVE_TYPE (((I2C_SLAVE_ADDR_LEN-1)<<4)|((I2C_SLAVE_REG_LEN-1)))

#define uint8 unsigned char
#define uint16 unsigned short
#define uint32 unsigned int
uint32 __sramdata data[5];
uint8 __sramdata arm_voltage = 0;
uint8 __sramdata vcode_voltage = 0;
uint8 __sramdata vddr_voltage = 0;
uint8 __sramdata axpreg12 = 0;
uint8 __sramdata vccio_voltage = 0;

#define CRU_CLKGATE0_CON   0xd0
#define CRU_CLKGATE8_CON	0xf0
#define CRU_CLKSEL1_CON		0x48
#define GRF_GPIO5H_IOMUX	0x74
#define GRF_GPIO2L_IOMUX    	0x58
#define GRF_GPIO1L_IOMUX    	0x50

#define COMPLETE_READ     (1<<STATE_START|1<<STATE_READ|1<<STATE_STOP)
#define COMPLETE_WRITE     (1<<STATE_START|1<<STATE_WRITE|1<<STATE_STOP)

/* Control register */
#define I2C_CON                 0x0000
#define I2C_CON_EN              (1 << 0)
#define I2C_CON_MOD(mod)        ((mod) << 1)
#define I2C_CON_MASK            (3 << 1)
enum{
        I2C_CON_MOD_TX = 0,
        I2C_CON_MOD_TRX,
        I2C_CON_MOD_RX,
        I2C_CON_MOD_RRX,
};
#define I2C_CON_START           (1 << 3)
#define I2C_CON_STOP            (1 << 4)
#define I2C_CON_LASTACK         (1 << 5)
#define I2C_CON_ACTACK          (1 << 6)

/* Clock dividor register */
#define I2C_CLKDIV              0x0004
#define I2C_CLKDIV_VAL(divl, divh) (((divl) & 0xffff) | (((divh) << 16) & 0xffff0000))  
#define rk30_ceil(x, y) \
	({ unsigned long __x = (x), __y = (y); (__x + __y - 1) / __y; })

/* the slave address accessed  for master rx mode */
#define I2C_MRXADDR             0x0008
#define I2C_MRXADDR_LOW         (1 << 24)
#define I2C_MRXADDR_MID         (1 << 25)
#define I2C_MRXADDR_HIGH        (1 << 26)

/* the slave register address accessed  for master rx mode */
#define I2C_MRXRADDR            0x000c
#define I2C_MRXRADDR_LOW        (1 << 24)
#define I2C_MRXRADDR_MID        (1 << 25)
#define I2C_MRXRADDR_HIGH       (1 << 26)

/* master tx count */
#define I2C_MTXCNT              0x0010

/* master rx count */
#define I2C_MRXCNT              0x0014

/* interrupt enable register */
#define I2C_IEN                 0x0018
#define I2C_BTFIEN              (1 << 0)
#define I2C_BRFIEN              (1 << 1)
#define I2C_MBTFIEN             (1 << 2)
#define I2C_MBRFIEN             (1 << 3)
#define I2C_STARTIEN            (1 << 4)
#define I2C_STOPIEN             (1 << 5)
#define I2C_NAKRCVIEN           (1 << 6)
#define IRQ_MST_ENABLE          (I2C_MBTFIEN | I2C_MBRFIEN | I2C_NAKRCVIEN | I2C_STARTIEN | I2C_STOPIEN)
#define IRQ_ALL_DISABLE         0

/* interrupt pending register */
#define I2C_IPD                 0x001c
#define I2C_BTFIPD              (1 << 0)
#define I2C_BRFIPD              (1 << 1)
#define I2C_MBTFIPD             (1 << 2)
#define I2C_MBRFIPD             (1 << 3)
#define I2C_STARTIPD            (1 << 4)
#define I2C_STOPIPD             (1 << 5)
#define I2C_NAKRCVIPD           (1 << 6)
#define I2C_IPD_ALL_CLEAN       0x7f

/* finished count */
#define I2C_FCNT                0x0020

/* I2C tx data register */
#define I2C_TXDATA_BASE         0X0100

/* I2C rx data register */
#define I2C_RXDATA_BASE         0x0200

void __sramfunc sram_i2c_enable(void);
void __sramfunc sram_i2c_disenable(void);

void __sramfunc sram_printch(char byte);
void __sramfunc sram_printhex(unsigned int hex);

#define sram_udelay(usecs)	SRAM_LOOP((usecs)*SRAM_LOOPS_PER_USEC)

/*-------------------------------------------------------------------------------------------------------
Name      : sram_i2c_init
Desc      : initialize the necessary registers
Params    : channel-determine which I2C bus we used
Return    : none
------------------------------------------------------------------------------------------------------*/

void __sramfunc sram_i2c_init(void)
{
	unsigned int div, divl, divh;
	//enable cru_clkgate8 clock
	data[1] = cru_readl(CLK_GATE_CLKID_CONS(CLK_GATE_CLKID(8)));

#if defined (CONFIG_MACH_RK2928_SDK) || defined( CONFIG_ARCH_RK3026_TB)||defined(CONFIG_ARCH_RK3028A_TB)
	cru_writel(CLK_GATE_W_MSK(CLK_GATE_PCLK_I2C1)|CLK_UN_GATE(CLK_GATE_PCLK_I2C1), 
	CLK_GATE_CLKID_CONS(CLK_GATE_PCLK_I2C1));
#else
	cru_writel(CLK_GATE_W_MSK(CLK_GATE_PCLK_I2C0)|CLK_UN_GATE(CLK_GATE_PCLK_I2C0), 
	CLK_GATE_CLKID_CONS(CLK_GATE_PCLK_I2C0));
#endif

	data[2] = readl_relaxed(RK2928_GRF_BASE + GRF_GPIO_IOMUX);
	writel_relaxed(data[2]| I2C_GRF_GPIO_IOMUX, RK2928_GRF_BASE + GRF_GPIO_IOMUX);

	div = 0x1e;
	divh = divl = 0xf;		
	writel_relaxed(I2C_CLKDIV_VAL(divl, divh), SRAM_I2C_ADDRBASE + I2C_CLKDIV);
	data[3] = readl_relaxed(SRAM_I2C_ADDRBASE + I2C_CLKDIV);
}


/*-------------------------------------------------------------------------------------------------------
Name      : sram_i2c_deinit
Desc      : de-initialize the necessary registers
Params    : noe
Return    : none
------------------------------------------------------------------------------------------------------*/
void __sramfunc sram_i2c_deinit(void)
{
    //restore iomux reg
    writel_relaxed(data[2], RK2928_GRF_BASE + GRF_GPIO_IOMUX);
    //restore scu clock reg
    cru_writel(data[1], CLK_GATE_CLKID_CONS(CLK_GATE_PCLK_I2C1));
}

/*-------------------------------------------------------------------------------------------------------
Name      : sram_i2c_start
Desc      : start i2c
Params    : none
Return    : none
------------------------------------------------------------------------------------------------------*/
void __sramfunc sram_i2c_read_enable(void)
{
	writel_relaxed(((((I2C_CON_EN | I2C_CON_MOD(1)) | I2C_CON_LASTACK) )| I2C_CON_START) & (~(I2C_CON_STOP)) , SRAM_I2C_ADDRBASE + I2C_CON);
}
void __sramfunc sram_i2c_write_enable(void)
{
	writel_relaxed(((((I2C_CON_EN | I2C_CON_MOD(0)) | I2C_CON_LASTACK) )| I2C_CON_START) & (~(I2C_CON_STOP)) , SRAM_I2C_ADDRBASE + I2C_CON);
}

void __sramfunc sram_i2c_disenable(void)
{
    writel_relaxed(0, SRAM_I2C_ADDRBASE + I2C_CON);
}

void __sramfunc sram_i2c_clean_start(void)
{
        unsigned int con = readl_relaxed(SRAM_I2C_ADDRBASE + I2C_CON);

        con = (con & (~I2C_CON_START)) ;
        writel_relaxed(con, SRAM_I2C_ADDRBASE + I2C_CON);
}
void __sramfunc sram_i2c_send_start(void)
{
	unsigned int con = readl_relaxed(SRAM_I2C_ADDRBASE + I2C_CON);
	con |= I2C_CON_START;
	if(con & I2C_CON_STOP)
		sram_printch('E');
	writel_relaxed(con, SRAM_I2C_ADDRBASE + I2C_CON);
}
void __sramfunc sram_i2c_send_stop(void)
{
	unsigned int con = readl_relaxed(SRAM_I2C_ADDRBASE + I2C_CON);
	con &= ~I2C_CON_START;
	con |= I2C_CON_STOP;
	if(con & I2C_CON_START) 
		sram_printch('E');
	writel_relaxed(con, SRAM_I2C_ADDRBASE + I2C_CON);
}
void __sramfunc sram_i2c_clean_stop(void)
{
        unsigned int con = readl_relaxed(SRAM_I2C_ADDRBASE + I2C_CON);

        con = (con & (~I2C_CON_STOP)) ;
        writel_relaxed(con, SRAM_I2C_ADDRBASE + I2C_CON);
}
void __sramfunc sram_i2c_get_ipd_event(int type)
{
	int time = 2000;
	unsigned int con = readl_relaxed(SRAM_I2C_ADDRBASE + I2C_IPD);
	writel_relaxed(type, SRAM_I2C_ADDRBASE + I2C_IEN);
	do{
		sram_udelay(10);
		con = readl_relaxed(SRAM_I2C_ADDRBASE + I2C_IPD);
	}while(((--time) & (~(con & type))));
	
	writel_relaxed(type,SRAM_I2C_ADDRBASE + I2C_IPD);
	if(time <= 0){
		sram_printch('T');
	}
}
void __sramfunc sram_i2c_write_prepare(uint8 I2CSlaveAddr, uint8 regAddr,uint8 pdata)
{
	u32 data = 0;
	unsigned int addr = (I2CSlaveAddr & 0x7f) << 1;

	data = (addr | (regAddr << 8))|(pdata << 16);
	writel_relaxed(data , SRAM_I2C_ADDRBASE + I2C_TXDATA_BASE);
	writel_relaxed(3, SRAM_I2C_ADDRBASE + I2C_MTXCNT);
}
uint8 __sramfunc sram_i2c_read_prepare(uint8 I2CSlaveAddr, uint8 regAddr)
{
	unsigned int addr = (I2CSlaveAddr & 0x7f) << 1;

	writel_relaxed(addr | I2C_MRXADDR_LOW, SRAM_I2C_ADDRBASE + I2C_MRXADDR);
	writel_relaxed(regAddr | I2C_MRXADDR_LOW, SRAM_I2C_ADDRBASE + I2C_MRXRADDR);
	writel_relaxed(SRAM_I2C_DATA_BYTE, SRAM_I2C_ADDRBASE + I2C_MRXCNT);
	return 0;
}
uint8 __sramfunc sram_i2c_read_get_data(uint8 I2CSlaveAddr, uint8 regAddr)
{
	unsigned int ret;
	ret = readl_relaxed(SRAM_I2C_ADDRBASE + I2C_RXDATA_BASE);
	ret = ret & 0x000000ff;
	return ret;
}
uint8 __sramfunc sram_i2c_write(uint8 I2CSlaveAddr, uint8 regAddr,uint8 data)
{
	sram_i2c_write_enable();
	sram_i2c_get_ipd_event(I2C_STARTIPD);
	sram_i2c_clean_start();
	
	sram_i2c_write_prepare(I2CSlaveAddr,regAddr,data);
	sram_i2c_get_ipd_event(I2C_MBTFIPD);
	
	sram_i2c_send_stop();
	sram_i2c_get_ipd_event(I2C_STOPIPD);
	sram_i2c_clean_stop();
	sram_i2c_disenable();
	return 0;
}
uint8 __sramfunc sram_i2c_read(uint8 I2CSlaveAddr, uint8 regAddr)
{
  	unsigned int data;
	sram_i2c_read_enable();
	sram_i2c_get_ipd_event(I2C_STARTIPD);
	sram_i2c_clean_start();

	sram_i2c_read_prepare(I2CSlaveAddr,regAddr);
	sram_i2c_get_ipd_event(I2C_MBRFIPD);
	
	data = sram_i2c_read_get_data(I2CSlaveAddr,regAddr);
	
 	sram_i2c_send_stop();
	sram_i2c_get_ipd_event(I2C_STOPIPD);
	sram_i2c_clean_stop();
	sram_i2c_disenable();
	return data;
}
#include <linux/suspend.h>
extern u32 __sramdata sram_pm_state;
void __sramfunc rk30_suspend_voltage_set(unsigned int vol)
{
	sram_i2c_init();
#if defined(CONFIG_KP_AXP22)
	/*//cpu arm to 0.95V*/
	arm_voltage = sram_i2c_read(0x34, 0x22);
	sram_i2c_write(0x34, 0x22, 0x9);//11

	//cpu code to 0.9V
	vcode_voltage = sram_i2c_read(0x34, 0x23);
	sram_i2c_write(0x34, 0x23, 0x0d);//0f

	//cpu ddr to 1.35V
	vddr_voltage = sram_i2c_read(0x34, 0x25);
	sram_i2c_write(0x34, 0x25, 0x05);//07
	vccio_voltage = sram_i2c_read(0x34, 0x21);
	sram_i2c_write(0x34, 0x21, 0x09);
	
#if defined(CONFIG_CLK_SWITCH_TO_GND)
	if (sram_pm_state == PM_SUSPEND_MEM)
	{
		//aldo3 VCCA_3V3
		sram_i2c_write(0x34, 0x13, 0x00);
		//ldo_ctl 关闭晶振
		sram_i2c_write(0x34, 0x16, 0x1f);
		axpreg12 = sram_i2c_read(0x34, 0x12);
		axpreg12 |= 0x10;	/*open dldo2 for sleep control*/
#if !defined(CONFIG_SPI)
		/* for xc3 tp, do not close tp power */
		axpreg12 |= 0x02;	/* open eldo2 TP_3.3V */
#endif
		sram_i2c_write(0x34, 0x12, axpreg12);
	}
#endif
#endif
//----------------------------------------------
#if defined(CONFIG_REGULATOR_ACT8931)	
	uint8 slaveaddr;
	uint8 slavereg;
	uint8 data;
	
	slaveaddr = 0x5b;
	{//ARM
		slavereg = 0x40;
		data = 0x0e;       //set arm 0.95v
		sram_i2c_read();
		(slaveaddr, slavereg, data);
		(slaveaddr,( slavereg+0x1), data);
	}
#if defined(CONFIG_CLK_SWITCH_TO_GND)
	if (sram_pm_state == PM_SUSPEND_MEM)
	{//LDO2
		slavereg = 0x55;
		data = 0xc1;       //ldo enable
		sram_i2c_write(slaveaddr, slavereg, data);
	}
#endif
#endif
	sram_i2c_deinit();
}
void __sramfunc rk30_suspend_voltage_resume(unsigned int vol)
{
	sram_i2c_init();
#if defined(CONFIG_KP_AXP22)
#if defined(CONFIG_CLK_SWITCH_TO_GND)
	if (sram_pm_state == PM_SUSPEND_MEM)
	{
#if defined(CONFIG_MACH_RK3026_E602)
		//aldo3 VCCA_3V3
		sram_i2c_write(0x34, 0x13, 0x80);
#endif
		axpreg12 &= (~0x10);	/*close dldo2 for sleep control*/
		sram_i2c_write(0x34, 0x12, axpreg12);
	}
#endif
	sram_i2c_write(0x34, 0x22, arm_voltage);
	sram_i2c_write(0x34, 0x23, vcode_voltage);
	sram_i2c_write(0x34, 0x25, vddr_voltage);
	sram_i2c_write(0x34, 0x21, vccio_voltage);
#endif

#if defined(CONFIG_REGULATOR_ACT8931)
	uint8 slaveaddr;
	uint8 slavereg;
	uint8 data;
	
	slaveaddr = 0x5b;
#if defined(CONFIG_CLK_SWITCH_TO_GND)
	if (sram_pm_state == PM_SUSPEND_MEM)
	{//LDO2
		slavereg = 0x55;
		data = 0x41;       //ldo disable
		sram_i2c_write(slaveaddr, slavereg, data);
	}
#endif
	{//ARM
		slavereg = 0x40;
		data = arm_voltage;
		sram_i2c_write(slaveaddr, slavereg, data);
		sram_i2c_write(slaveaddr, (slavereg+0x1), data);
		sram_udelay(20000);
	}
#endif
	sram_i2c_deinit();
}
#else
void rk30_suspend_voltage_set(unsigned int vol){}
void rk30_suspend_voltage_resume(unsigned int vol){}
#endif



