/*
 * Battery charger driver for X-Powers AXP
 *
 * Copyright (C) 2013 X-Powers, Ltd.
 *  Zhang Donglu <zhangdonglu@x-powers.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#include <asm/div64.h>

/*#include <asm-generic/gpio.h>*/
#include <mach/gpio.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "axp-cfg.h"
#include "axp-sply.h"

#include <linux/gpio.h>
#include <mach/board.h>

#define DBG_AXP_PSY 0
#if  DBG_AXP_PSY
#define DBG_PSY_MSG(format,args...)   printk(KERN_INFO "[AXP22]"format,##args)
#else
#define DBG_PSY_MSG(format,args...)   do {} while (0)
#endif

static int axp_debug = 0;
static uint8_t axp_reg_addr = 0;
struct axp_adc_res adc;
struct delayed_work usbwork;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend axp_early_suspend;
int early_suspend_flag = 0;
#endif
/*int system_lowerpower_nocharger = 0;*/
/*int system_lowerpower = 0;*/

int axp_shortpress_flag = 0;
EXPORT_SYMBOL_GPL(axp_shortpress_flag);

int manual_suspend_flag = 0;
EXPORT_SYMBOL_GPL(manual_suspend_flag);

int pmu_usbvolnew = 0;
int pmu_usbcurnew = 0;
int axp_usbcurflag = 0;
int axp_usbvolflag = 0;

int axp_chip_id_get(uint8_t chip_id[16])
{
	uint8_t ret;
	ret = axp_write(axp_charger->master,0xff,0x01);
	if(ret)
	{
		printk("[axp22x] axp22x write REG_ff fail!");
	}
	axp_reads(axp_charger->master,0x20,16,chip_id);
	if(ret)
	{
		printk("[axp22x] axp22x reads REG_12x fail!");
	}
	axp_write(axp_charger->master,0xff,0x00);
	if(ret)
	{
		printk("[axp22x] axp22x write REG_ff fail!");
	}
#if 0
	for(i=0;i<16;i++)
	{
		printk("axp22x REG12%x=%x\n",i,chip_id[i]);
	}
#endif
    return ret;
}
EXPORT_SYMBOL_GPL(axp_chip_id_get);
/*控制usb电压的开关，期望在外接usb host 充电时调用*/
int axp_usbvol(void)
{
	axp_usbvolflag = 1;
    return 0;
}
EXPORT_SYMBOL_GPL(axp_usbvol);

int axp_usb_det(void)
{
	uint8_t ret;
	axp_read(axp_charger->master,AXP22_CHARGE_STATUS,&ret);
	if(ret & 0x10)/*usb or usb adapter can be used*/
	{
		return 1;
	}
	else/*no usb or usb adapter*/
	{
		return 0;
	}
}
EXPORT_SYMBOL_GPL(axp_usb_det);

/*控制usb电流的开关，期望在外接usb host 充电时调用*/
int axp_usbcur(void)
{
    axp_usbcurflag = 1;
    return 0;
}
EXPORT_SYMBOL_GPL(axp_usbcur);
/*控制usb电压的开关，期望在从usb host 拔出时调用*/
int axp_usbvol_restore(void)
{
 	axp_usbvolflag = 0;
    return 0;
}
EXPORT_SYMBOL_GPL(axp_usbvol_restore);
/*控制usb电流的开关，期望在从usb host 拔出时调用*/
int axp_usbcur_restore(void)
{
	axp_usbcurflag = 0;
    return 0;
}
EXPORT_SYMBOL_GPL(axp_usbcur_restore);

/*调试接口*/
static ssize_t axp_reg_show(struct class *class, struct class_attribute *attr, char *buf)
{
    uint8_t val;
    axp_read(axp_charger->master,axp_reg_addr,&val);
    return sprintf(buf,"REG[%x]=%x\n",axp_reg_addr,val);
}

static ssize_t axp_reg_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int tmp;
    uint8_t val;
    tmp = simple_strtoul(buf, NULL, 16);
    if( tmp < 256 )
    	axp_reg_addr = tmp;
    else {
    	val = tmp & 0x00FF;
    	axp_reg_addr= (tmp >> 8) & 0x00FF;
    	axp_write(axp_charger->master,axp_reg_addr, val);
    }
    return count;
}

static ssize_t axp_regs_show(struct class *class, struct class_attribute *attr, char *buf)
{
    uint8_t val[4];
    axp_reads(axp_charger->master,axp_reg_addr,4,val);
    return sprintf(buf,"REG[0x%x]=0x%x,REG[0x%x]=0x%x,REG[0x%x]=0x%x,REG[0x%x]=0x%x\n",axp_reg_addr,val[0],axp_reg_addr+1,val[1],axp_reg_addr+2,val[2],axp_reg_addr+3,val[3]);
}

static ssize_t axp_regs_store(struct class *class,struct class_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val[5];
	tmp = simple_strtoul(buf, NULL, 16);
	if( tmp < 256 )
        	axp_reg_addr = tmp;
	else {
		axp_reg_addr= (tmp >> 24) & 0xFF;
		val[0] = (tmp >> 16) & 0xFF;
		val[1] =  axp_reg_addr + 1;
		val[2] = (tmp >>  8)& 0xFF;
		val[3] =  axp_reg_addr + 2;
		val[4] = (tmp >>  0)& 0xFF;
		axp_writes(axp_charger->master,axp_reg_addr,5,val);
	}
	return count;
}

static ssize_t axpdebug_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    if(buf[0] == '1'){
        axp_debug = 1; 
    }
    else{
        axp_debug = 0;         
    }        
    return count;
}

static ssize_t axpdebug_show(struct class *class,struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "bat-debug value is %d\n", axp_debug);
}

static struct class_attribute axppower_class_attrs[] = {
    __ATTR(axpdebug,S_IRUGO |S_IWUSR,axpdebug_show,axpdebug_store),
    __ATTR(axpreg,  S_IRUGO |S_IWUSR,axp_reg_show,axp_reg_store),
    __ATTR(axpregs, S_IRUGO |S_IWUSR,axp_regs_show,axp_regs_store),
    __ATTR_NULL
};

static struct class axppower_class = {
    .name = "axppower",
    .class_attrs = axppower_class_attrs,
};

int ADC_Freq_Get(struct axp_charger *charger)
{
	uint8_t  temp;
	int  rValue = 25;

	axp_read(charger->master, AXP22_ADC_CONTROL3,&temp);
	temp &= 0xc0;
	switch(temp >> 6)
	{
		case 0:
			rValue = 100;
			break;
		case 1:
			rValue = 200;
			break;
		case 2:
			rValue = 400;
			break;
		case 3:
			rValue = 800;
			break;
		default:
			break;
	}
	return rValue;
}

static inline int axp22_vbat_to_mV(uint16_t reg)
{
  return ((int)((( reg >> 8) << 4 ) | (reg & 0x000F))) * 1100 / 1000;
}

static inline int axp22_ocvbat_to_mV(uint16_t reg)
{
  return ((int)((( reg >> 8) << 4 ) | (reg & 0x000F))) * 1100 / 1000;
}


static inline int axp22_vdc_to_mV(uint16_t reg)
{
  return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) * 1700 / 1000;
}


static inline int axp22_ibat_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) ;
}

static inline int axp22_icharge_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F)));
}

static inline int axp22_iac_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) * 625 / 1000;
}

static inline int axp22_iusb_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) * 375 / 1000;
}


static inline void axp_read_adc(struct axp_charger *charger,
  struct axp_adc_res *adc)
{
  uint8_t tmp[8];
//
//  axp_reads(charger->master,AXP22_VACH_RES,8,tmp);
  adc->vac_res = 0;
  adc->iac_res = 0;
  adc->vusb_res = 0;
  adc->iusb_res = 0;
  axp_reads(charger->master,AXP22_VBATH_RES,6,tmp);
  adc->vbat_res = ((uint16_t) tmp[0] << 8 )| tmp[1];
  adc->ichar_res = ((uint16_t) tmp[2] << 8 )| tmp[3];
  adc->idischar_res = ((uint16_t) tmp[4] << 8 )| tmp[5];
  axp_reads(charger->master,AXP22_OCVBATH_RES,2,tmp);
  adc->ocvbat_res = ((uint16_t) tmp[0] << 8 )| tmp[1];
}


static void axp_charger_update_state(struct axp_charger *charger)
{
  uint8_t val[2];
  uint16_t tmp;
  axp_reads(charger->master,AXP22_CHARGE_STATUS,2,val);
  tmp = (val[1] << 8 )+ val[0];
  charger->is_on = (val[1] & AXP22_IN_CHARGE) ? 1 : 0;
  charger->fault = val[1];
  charger->bat_det = (tmp & AXP22_STATUS_BATEN)?1:0;
  charger->ac_det = (tmp & AXP22_STATUS_ACEN)?1:0;
  charger->usb_det = (tmp & AXP22_STATUS_USBEN)?1:0;
  charger->usb_valid = (tmp & AXP22_STATUS_USBVA)?1:0;
  charger->ac_valid = (tmp & AXP22_STATUS_ACVA)?1:0;
  charger->ext_valid = charger->ac_valid | charger->usb_valid;
  charger->bat_current_direction = (tmp & AXP22_STATUS_BATCURDIR)?1:0;
  charger->in_short = (tmp& AXP22_STATUS_ACUSBSH)?1:0;
  charger->batery_active = (tmp & AXP22_STATUS_BATINACT)?1:0;
  charger->int_over_temp = (tmp & AXP22_STATUS_ICTEMOV)?1:0;
  axp_read(charger->master,AXP22_CHARGE_CONTROL1,val);
  charger->charge_on = ((val[0] >> 7) & 0x01);
}

static void axp_charger_update(struct axp_charger *charger)
{
  uint16_t tmp;
  uint8_t val[2];
  //struct axp_adc_res adc;
  charger->adc = &adc;
  axp_read_adc(charger, &adc);
  tmp = charger->adc->vbat_res;
  charger->vbat = axp22_vbat_to_mV(tmp);
  tmp = charger->adc->ocvbat_res;
  charger->ocv = axp22_ocvbat_to_mV(tmp);
   //tmp = charger->adc->ichar_res + charger->adc->idischar_res;
  charger->ibat = ABS(axp22_icharge_to_mA(charger->adc->ichar_res)-axp22_ibat_to_mA(charger->adc->idischar_res));
  tmp = 00;
  charger->vac = axp22_vdc_to_mV(tmp);
  tmp = 00;
  charger->iac = axp22_iac_to_mA(tmp);
  tmp = 00;
  charger->vusb = axp22_vdc_to_mV(tmp);
  tmp = 00;
  charger->iusb = axp22_iusb_to_mA(tmp);
  axp_reads(charger->master,AXP22_INTTEMP,2,val);
  //DBG_PSY_MSG("TEMPERATURE:val1=0x%x,val2=0x%x\n",val[1],val[0]);
  tmp = (val[0] << 4 ) + (val[1] & 0x0F);
  charger->ic_temp = (int) tmp *1063/10000  - 2667/10;
  charger->disvbat =  charger->vbat;
  charger->disibat =  axp22_ibat_to_mA(charger->adc->idischar_res);
}

#if defined  (CONFIG_AXP_CHARGEINIT)
static void axp_set_charge(struct axp_charger *charger)
{
  uint8_t val=0x00;
  uint8_t tmp=0x00;
    if(charger->chgvol < 4200000){
      val &= ~(3 << 5);
	  //val |= 1 << 5;
      }
    else if (charger->chgvol<4220000)		
		{
			  val &= ~(3 << 5);
			  val |= 1 << 6;
		}

    else if (charger->chgvol<4240000){
      val &= ~(3 << 5);
      val |= 1 << 5;
      }
    else
      val |= 3 << 5;

		if(charger->chgcur == 0)
			charger->chgen = 0;

    if(charger->chgcur< 300000)
      charger->chgcur = 300000;
    else if(charger->chgcur > 2550000)
     charger->chgcur = 2550000;

    val |= (charger->chgcur - 300000) / 150000 ;
    if(charger ->chgend == 10){
      val &= ~(1 << 4);
    }
    else {
      val |= 1 << 4;
    }
    val &= 0x7F;
    val |= charger->chgen << 7;
      if(charger->chgpretime < 30)
      charger->chgpretime = 30;
    if(charger->chgcsttime < 360)
      charger->chgcsttime = 360;

    tmp = ((((charger->chgpretime - 40) / 10) << 6)  \
      | ((charger->chgcsttime - 360) / 120));
	axp_write(charger->master, AXP22_CHARGE_CONTROL1,val);
	axp_update(charger->master, AXP22_CHARGE_CONTROL2,tmp,0xC2);
}
#else
static void axp_set_charge(struct axp_charger *charger)
{

}
#endif

static enum power_supply_property axp_battery_props[] = {
  POWER_SUPPLY_PROP_MODEL_NAME,
  POWER_SUPPLY_PROP_STATUS,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_ONLINE,
  POWER_SUPPLY_PROP_HEALTH,
  POWER_SUPPLY_PROP_TECHNOLOGY,
  POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
  POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
  POWER_SUPPLY_PROP_VOLTAGE_NOW,
  POWER_SUPPLY_PROP_CURRENT_NOW,
  POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
  //POWER_SUPPLY_PROP_CHARGE_FULL,
  POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
  POWER_SUPPLY_PROP_CAPACITY,
  //POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
  //POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
  POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property axp_ac_props[] = {
  POWER_SUPPLY_PROP_MODEL_NAME,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_ONLINE,
  POWER_SUPPLY_PROP_VOLTAGE_NOW,
  POWER_SUPPLY_PROP_CURRENT_NOW,
};

static enum power_supply_property axp_usb_props[] = {
  POWER_SUPPLY_PROP_MODEL_NAME,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_ONLINE,
  POWER_SUPPLY_PROP_VOLTAGE_NOW,
  POWER_SUPPLY_PROP_CURRENT_NOW,
};

static void axp_battery_check_status(struct axp_charger *charger,
            union power_supply_propval *val)
{
  if (charger->bat_det) {
    if (charger->ext_valid){
    	if( charger->rest_vol == 100)
        val->intval = POWER_SUPPLY_STATUS_FULL;
    	else if(charger->charge_on)
    		val->intval = POWER_SUPPLY_STATUS_CHARGING;
    	else
    		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
    }
    else
      val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
  }
  else
    val->intval = POWER_SUPPLY_STATUS_FULL;
}

static void axp_battery_check_health(struct axp_charger *charger,
            union power_supply_propval *val)
{
    if (charger->fault & AXP22_FAULT_LOG_BATINACT)
    val->intval = POWER_SUPPLY_HEALTH_DEAD;
  else if (charger->fault & AXP22_FAULT_LOG_OVER_TEMP)
    val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
  else if (charger->fault & AXP22_FAULT_LOG_COLD)
    val->intval = POWER_SUPPLY_HEALTH_COLD;
  else
    val->intval = POWER_SUPPLY_HEALTH_GOOD;
}

static int axp_battery_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
  struct axp_charger *charger;
  int ret = 0;
  charger = container_of(psy, struct axp_charger, batt);

  switch (psp) {
  case POWER_SUPPLY_PROP_STATUS:
    axp_battery_check_status(charger, val);
    break;
  case POWER_SUPPLY_PROP_HEALTH:
    axp_battery_check_health(charger, val);
    break;
  case POWER_SUPPLY_PROP_TECHNOLOGY:
    val->intval = charger->battery_info->technology;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
    val->intval = charger->battery_info->voltage_max_design;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
    val->intval = charger->battery_info->voltage_min_design;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_NOW:
    val->intval = charger->vbat * 1000;
    break;
  case POWER_SUPPLY_PROP_CURRENT_NOW:
    val->intval = charger->ibat * 1000;
    break;
  case POWER_SUPPLY_PROP_MODEL_NAME:
    val->strval = charger->batt.name;
    break;
/*  case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
  case POWER_SUPPLY_PROP_CHARGE_FULL:
    val->intval = charger->battery_info->charge_full_design;
        break;
*/
  case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
  case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
    val->intval = charger->battery_info->energy_full_design;
  //  DBG_PSY_MSG("POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:%d\n",val->intval);
       break;
  case POWER_SUPPLY_PROP_CAPACITY:
    val->intval = charger->rest_vol;
    break;
/*  case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
    if(charger->bat_det && !(charger->is_on) && !(charger->ext_valid))
      val->intval = charger->rest_time;
    else
      val->intval = 0;
    break;
  case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
    if(charger->bat_det && charger->is_on)
      val->intval = charger->rest_time;
    else
      val->intval = 0;
    break;
*/
  case POWER_SUPPLY_PROP_ONLINE:
  {
    axp_charger_update_state(charger);
    val->intval = charger->bat_current_direction;
    printk("axp battery hardware current direction %d\n", charger->bat_current_direction);
    break;
  }
  case POWER_SUPPLY_PROP_PRESENT:
    val->intval = charger->bat_det;
    break;
  case POWER_SUPPLY_PROP_TEMP:
    //val->intval = charger->ic_temp - 200;
    val->intval =  300;
    break;
  default:
    ret = -EINVAL;
    break;
  }

  return ret;
}

static int axp_ac_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
  struct axp_charger *charger;
  int ret = 0;
  charger = container_of(psy, struct axp_charger, ac);

  switch(psp){
  case POWER_SUPPLY_PROP_MODEL_NAME:
    val->strval = charger->ac.name;break;
  case POWER_SUPPLY_PROP_PRESENT:
    val->intval = charger->ac_det;
    break;
  case POWER_SUPPLY_PROP_ONLINE:
    val->intval = charger->ac_valid;break;
  case POWER_SUPPLY_PROP_VOLTAGE_NOW:
    val->intval = charger->vac * 1000;
    break;
  case POWER_SUPPLY_PROP_CURRENT_NOW:
    val->intval = charger->iac * 1000;
    break;
  default:
    ret = -EINVAL;
    break;
  }
   return ret;
}

static int axp_usb_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
  struct axp_charger *charger;
  int ret = 0;
  charger = container_of(psy, struct axp_charger, usb);

  switch(psp){
  case POWER_SUPPLY_PROP_MODEL_NAME:
    val->strval = charger->usb.name;break;
  case POWER_SUPPLY_PROP_PRESENT:
    val->intval = charger->usb_det;
    break;
  case POWER_SUPPLY_PROP_ONLINE:
    val->intval = charger->usb_valid;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_NOW:
    val->intval = charger->vusb * 1000;
    break;
  case POWER_SUPPLY_PROP_CURRENT_NOW:
    val->intval = charger->iusb * 1000;
    break;
  default:
    ret = -EINVAL;
    break;
  }
   return ret;
}

static void axp_change(struct axp_charger *charger)
{
  uint8_t val,tmp;
  int var;
  DBG_PSY_MSG("battery state change\n");
  axp_charger_update_state(charger);
  axp_charger_update(charger);
  printk("charger->usb_valid = %d\n",charger->usb_valid);
	if(!charger->usb_valid){
		printk("set usb vol-lim to %d mV, cur-lim to %d mA\n",USBVOLLIM,USBCURLIM);
        //cancel_delayed_work_sync(&usbwork);
		//reset usb-ac after usb removed 
		if((USBCURLIM) && (USBCURLIMEN)){
			axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x01);
			var = USBCURLIM * 1000;
			if(var >= 900000)
				axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x03);
			else if ((var >= 500000)&& (var < 900000)){
				axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x02);
				axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x01);
			}
			else if ((var >= 100000)&& (var < 500000)){
				axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x01);
				axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x02);
			}
			else
				printk("set usb limit current error,%d mA\n",USBCURLIM);	
		}
		else
			axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x03);
			
		if((USBVOLLIM) && (USBVOLLIMEN)){
			axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x40);
			var = USBVOLLIM * 1000;
			if(var >= 4000000 && var <=4700000){
				tmp = (var - 4000000)/100000;
			    axp_read(charger->master, AXP22_CHARGE_VBUS,&val);
			    val &= 0xC7;
			    val |= tmp << 3;
			    axp_write(charger->master, AXP22_CHARGE_VBUS,val);
			}
			else
				printk("set usb limit voltage error,%d mV\n",USBVOLLIM);	
		}
		else
			axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x40);
	}
  flag_state_change = 1;
  power_supply_changed(&charger->batt);
}

static void axp_presslong(struct axp_charger *charger)
{
	DBG_PSY_MSG("press long\n");
	input_report_key(powerkeydev, KEY_POWER, 1);
	input_sync(powerkeydev);
	ssleep(2);
	DBG_PSY_MSG("press long up\n");
	LOG("press long up");
	input_report_key(powerkeydev, KEY_POWER, 0);
	input_sync(powerkeydev);
}

static void axp_pressshort(struct axp_charger *charger)
{
	DBG_PSY_MSG("press short\n");
	LOG("press short");
	axp_shortpress_flag = 1;
  	input_report_key(powerkeydev, KEY_POWER, 1);
 	input_sync(powerkeydev);
 	msleep(10);
 	input_report_key(powerkeydev, KEY_POWER, 0);
 	input_sync(powerkeydev);
}

static void axp_keyup(struct axp_charger *charger)
{
	DBG_PSY_MSG("power key up\n");
	LOG("power key up");
	input_report_key(powerkeydev, KEY_POWER, 0);
	input_sync(powerkeydev);
}

static void axp_keydown(struct axp_charger *charger)
{
	DBG_PSY_MSG("power key down\n");
	LOG("power key down");
	input_report_key(powerkeydev, KEY_POWER, 1);
	input_sync(powerkeydev);
}

#if 0
void axp_send_wakeup_key(void)
{
	DBG_PSY_MSG("send wakeup key\n");
	if(unlikely(!axp_charger)) {
		printk("%s,axp_charger is NULL!\n", __func__);
		return;
	}

	input_report_key(powerkeydev, KEY_WAKEUP, 1);
	input_sync(powerkeydev);
	msleep(100);
	input_report_key(powerkeydev, KEY_WAKEUP, 0);
	input_sync(powerkeydev);
	wake_lock_timeout(&axp_charger->axp_lock, 3 * HZ); /* 3s */
}
#endif

void rk_send_wakeup_key(void)
{
	printk("send rockchip wakeup key\n");
	if(unlikely(!axp_charger)) {
		printk("%s,axp_charger is NULL!\n", __func__);
		return;
	}
	rk28_send_wakeup_key();
	wake_lock_timeout(&axp_charger->axp_lock, 5 * HZ); /* 5s */
}

static void axp_capchange(struct axp_charger *charger)
{
    uint8_t val;
    int k;

    DBG_PSY_MSG("battery change\n");
    ssleep(2);
    axp_charger_update_state(charger);
    axp_charger_update(charger);
    axp_read(charger->master, AXP22_CAP,&val);
    charger->rest_vol = (int) (val & 0x7F);

    if((charger->bat_det == 0) || (charger->rest_vol == 127)){
  	charger->rest_vol = 100;
  }

  DBG_PSY_MSG("rest_vol = %d\n",charger->rest_vol);
  memset(Bat_Cap_Buffer, 0, sizeof(Bat_Cap_Buffer));
  for(k = 0;k < AXP_VOL_MAX; k++){
    Bat_Cap_Buffer[k] = charger->rest_vol;
  }
  Total_Cap = charger->rest_vol * AXP_VOL_MAX;
  power_supply_changed(&charger->batt);
}

static int axp_battery_event(struct notifier_block *nb, unsigned long event,
        void *data)
{
    struct axp_charger *charger =
    container_of(nb, struct axp_charger, nb);

    uint8_t w[9];
	if(axp_debug){
		DBG_PSY_MSG("axp_battery_event enter...\n");
	}
    if((bool)data==0){
		if(axp_debug){
    		DBG_PSY_MSG("low 32bit status...\n");
		}
			if(event & (AXP22_IRQ_BATIN|AXP22_IRQ_BATRE)) {
				axp_capchange(charger);
			}
	
			if(event & (AXP22_IRQ_ACIN|AXP22_IRQ_USBIN|AXP22_IRQ_ACOV|AXP22_IRQ_USBOV|AXP22_IRQ_CHAOV
						|AXP22_IRQ_CHAST|AXP22_IRQ_TEMOV|AXP22_IRQ_TEMLO)) {
				axp_change(charger);
			}
	
			if(event & (AXP22_IRQ_ACRE|AXP22_IRQ_USBRE)) {
				axp_change(charger);
			}

			if(event & AXP22_IRQ_USBIN) {
				DBG_PSY_MSG("+++++ usb in +++++\n");
			}

			if(event & AXP22_IRQ_USBRE) {
				DBG_PSY_MSG("+++++ usb out +++++\n");
			}
	
			if(event & AXP22_IRQ_POKLO) {
				axp_presslong(charger);
			}
	
			if(event & AXP22_IRQ_POKSH) {
				axp_pressshort(charger);
			}
#if 0
			/* warning */
			if(event & AXP_IRQ_EXTLOWARN1) {
				rk_send_wakeup_key();
			}
#endif
			/* shutdown */
			if(event & AXP_IRQ_EXTLOWARN2) {
				//axp_pressshort(charger);
				rk_send_wakeup_key();
			}

			w[0] = (uint8_t) ((event) & 0xFF);
    		w[1] = AXP22_INTSTS2;
    		w[2] = (uint8_t) ((event >> 8) & 0xFF);
    		w[3] = AXP22_INTSTS3;
    		w[4] = (uint8_t) ((event >> 16) & 0xFF);
    		w[5] = AXP22_INTSTS4;
    		w[6] = (uint8_t) ((event >> 24) & 0xFF);
    		w[7] = AXP22_INTSTS5;
    		w[8] = 0;
	} else {
		if((event) & (AXP22_IRQ_PEKFE>>32)) {
			axp_keydown(charger);
		}

		if((event) & (AXP22_IRQ_PEKRE>>32)) {
			axp_keyup(charger);
		}
		if(axp_debug){
			DBG_PSY_MSG("high 32bit status...\n");
		}
		w[0] = 0;
    	w[1] = AXP22_INTSTS2;
    	w[2] = 0;
    	w[3] = AXP22_INTSTS3;
    	w[4] = 0;
    	w[5] = AXP22_INTSTS4;
    	w[6] = 0;
    	w[7] = AXP22_INTSTS5;
    	w[8] = (uint8_t) ((event) & 0xFF);;
	}
    DBG_PSY_MSG("event = 0x%x\n",(int) event);
    axp_writes(charger->master,AXP22_INTSTS1,9,w);

    return 0;
}

static char *supply_list[] = {
  "battery",
};



static void axp_battery_setup_psy(struct axp_charger *charger)
{
  struct power_supply *batt = &charger->batt;
  struct power_supply *ac = &charger->ac;
  struct power_supply *usb = &charger->usb;
  struct power_supply_info *info = charger->battery_info;

  batt->name = "battery";
  batt->use_for_apm = info->use_for_apm;
  batt->type = POWER_SUPPLY_TYPE_BATTERY;
  batt->get_property = axp_battery_get_property;

  batt->properties = axp_battery_props;
  batt->num_properties = ARRAY_SIZE(axp_battery_props);

  ac->name = "ac";
  ac->type = POWER_SUPPLY_TYPE_MAINS;
  ac->get_property = axp_ac_get_property;

  ac->supplied_to = supply_list,
  ac->num_supplicants = ARRAY_SIZE(supply_list),

  ac->properties = axp_ac_props;
  ac->num_properties = ARRAY_SIZE(axp_ac_props);

  usb->name = "usb";
  usb->type = POWER_SUPPLY_TYPE_USB;
  usb->get_property = axp_usb_get_property;

  usb->supplied_to = supply_list,
  usb->num_supplicants = ARRAY_SIZE(supply_list),

  usb->properties = axp_usb_props;
  usb->num_properties = ARRAY_SIZE(axp_usb_props);
};

#if defined  (CONFIG_AXP_CHARGEINIT)
static int axp_battery_adc_set(struct axp_charger *charger)
{
   int ret ;
   uint8_t val;

  /*enable adc and set adc */
  val= AXP22_ADC_BATVOL_ENABLE | AXP22_ADC_BATCUR_ENABLE;

	ret = axp_update(charger->master, AXP22_ADC_CONTROL, val , val);
  if (ret)
    return ret;
    ret = axp_read(charger->master, AXP22_ADC_CONTROL3, &val);
  switch (charger->sample_time/100){
  case 1: val &= ~(3 << 6);break;
  case 2: val &= ~(3 << 6);val |= 1 << 6;break;
  case 4: val &= ~(3 << 6);val |= 2 << 6;break;
  case 8: val |= 3 << 6;break;
  default: break;
  }
  ret = axp_write(charger->master, AXP22_ADC_CONTROL3, val);
  if (ret)
    return ret;

  return 0;
}
#else
static int axp_battery_adc_set(struct axp_charger *charger)
{
  return 0;
}
#endif

static int axp_battery_first_init(struct axp_charger *charger)
{
   int ret;
   uint8_t val;
   axp_set_charge(charger);
   ret = axp_battery_adc_set(charger);
   if(ret)
    return ret;

   ret = axp_read(charger->master, AXP22_ADC_CONTROL3, &val);
   switch ((val >> 6) & 0x03){
  case 0: charger->sample_time = 100;break;
  case 1: charger->sample_time = 200;break;
  case 2: charger->sample_time = 400;break;
  case 3: charger->sample_time = 800;break;
  default:break;
  }
  return ret;
}
#if defined (CONFIG_AXP_DEBUG)
static ssize_t chgen_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master, AXP22_CHARGE_CONTROL1, &val);
  charger->chgen  = val >> 7;
  return sprintf(buf, "%d\n",charger->chgen);
}

static ssize_t chgen_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var){
    charger->chgen = 1;
    axp_set_bits(charger->master,AXP22_CHARGE_CONTROL1,0x80);
  }
  else{
    charger->chgen = 0;
    axp_clr_bits(charger->master,AXP22_CHARGE_CONTROL1,0x80);
  }
  return count;
}

static ssize_t chgmicrovol_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master, AXP22_CHARGE_CONTROL1, &val);
  switch ((val >> 5) & 0x03){
    case 0: charger->chgvol = 4100000;break;
    case 1: charger->chgvol = 4220000;break;
    case 2: charger->chgvol = 4200000;break;
    case 3: charger->chgvol = 4240000;break;
  }
  return sprintf(buf, "%d\n",charger->chgvol);
}

static ssize_t chgmicrovol_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t tmp, val;
  var = simple_strtoul(buf, NULL, 10);
  switch(var){
    case 4100000:tmp = 0;break;
    case 4220000:tmp = 1;break;
    case 4200000:tmp = 2;break;
    case 4240000:tmp = 3;break;
    default:  tmp = 4;break;
  }
  if(tmp < 4){
    charger->chgvol = var;
    axp_read(charger->master, AXP22_CHARGE_CONTROL1, &val);
    val &= 0x9F;
    val |= tmp << 5;
    axp_write(charger->master, AXP22_CHARGE_CONTROL1, val);
  }
  return count;
}

static ssize_t chgintmicrocur_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master, AXP22_CHARGE_CONTROL1, &val);
  charger->chgcur = (val & 0x0F) * 150000 +300000;
  return sprintf(buf, "%d\n",charger->chgcur);
}

static ssize_t chgintmicrocur_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t val,tmp;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 300000 && var <= 2550000){
    tmp = (var -200001)/150000;
    charger->chgcur = tmp *150000 + 300000;
    axp_read(charger->master, AXP22_CHARGE_CONTROL1, &val);
    val &= 0xF0;
    val |= tmp;
    axp_write(charger->master, AXP22_CHARGE_CONTROL1, val);
  }
  return count;
}

static ssize_t chgendcur_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master, AXP22_CHARGE_CONTROL1, &val);
  charger->chgend = ((val >> 4)& 0x01)? 15 : 10;
  return sprintf(buf, "%d\n",charger->chgend);
}

static ssize_t chgendcur_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var == 10 ){
    charger->chgend = var;
    axp_clr_bits(charger->master ,AXP22_CHARGE_CONTROL1,0x10);
  }
  else if (var == 15){
    charger->chgend = var;
    axp_set_bits(charger->master ,AXP22_CHARGE_CONTROL1,0x10);

  }
  return count;
}

static ssize_t chgpretimemin_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master,AXP22_CHARGE_CONTROL2, &val);
  charger->chgpretime = (val >> 6) * 10 +40;
  return sprintf(buf, "%d\n",charger->chgpretime);
}

static ssize_t chgpretimemin_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t tmp,val;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 40 && var <= 70){
    tmp = (var - 40)/10;
    charger->chgpretime = tmp * 10 + 40;
    axp_read(charger->master,AXP22_CHARGE_CONTROL2,&val);
    val &= 0x3F;
    val |= (tmp << 6);
    axp_write(charger->master,AXP22_CHARGE_CONTROL2,val);
  }
  return count;
}

static ssize_t chgcsttimemin_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master,AXP22_CHARGE_CONTROL2, &val);
  charger->chgcsttime = (val & 0x03) *120 + 360;
  return sprintf(buf, "%d\n",charger->chgcsttime);
}

static ssize_t chgcsttimemin_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t tmp,val;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 360 && var <= 720){
    tmp = (var - 360)/120;
    charger->chgcsttime = tmp * 120 + 360;
    axp_read(charger->master,AXP22_CHARGE_CONTROL2,&val);
    val &= 0xFC;
    val |= tmp;
    axp_write(charger->master,AXP22_CHARGE_CONTROL2,val);
  }
  return count;
}

static ssize_t adcfreq_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master, AXP22_ADC_CONTROL3, &val);
  switch ((val >> 6) & 0x03){
     case 0: charger->sample_time = 100;break;
     case 1: charger->sample_time = 200;break;
     case 2: charger->sample_time = 400;break;
     case 3: charger->sample_time = 800;break;
     default:break;
  }
  return sprintf(buf, "%d\n",charger->sample_time);
}

static ssize_t adcfreq_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t val;
  var = simple_strtoul(buf, NULL, 10);
  axp_read(charger->master, AXP22_ADC_CONTROL3, &val);
  switch (var/25){
    case 1: val &= ~(3 << 6);charger->sample_time = 100;break;
    case 2: val &= ~(3 << 6);val |= 1 << 6;charger->sample_time = 200;break;
    case 4: val &= ~(3 << 6);val |= 2 << 6;charger->sample_time = 400;break;
    case 8: val |= 3 << 6;charger->sample_time = 800;break;
    default: break;
    }
  axp_write(charger->master, AXP22_ADC_CONTROL3, val);
  return count;
}


static ssize_t vholden_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master,AXP22_CHARGE_VBUS, &val);
  val = (val>>6) & 0x01;
  return sprintf(buf, "%d\n",val);
}

static ssize_t vholden_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var)
    axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x40);
  else
    axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x40);

  return count;
}

static ssize_t vhold_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  int vhold;
  axp_read(charger->master,AXP22_CHARGE_VBUS, &val);
  vhold = ((val >> 3) & 0x07) * 100000 + 4000000;
  return sprintf(buf, "%d\n",vhold);
}

static ssize_t vhold_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t val,tmp;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 4000000 && var <=4700000){
    tmp = (var - 4000000)/100000;
    //printk("tmp = 0x%x\n",tmp);
    axp_read(charger->master, AXP22_CHARGE_VBUS,&val);
    val &= 0xC7;
    val |= tmp << 3;
    //printk("val = 0x%x\n",val);
    axp_write(charger->master, AXP22_CHARGE_VBUS,val);
  }
  return count;
}

static ssize_t iholden_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master,AXP22_CHARGE_VBUS, &val);
  return sprintf(buf, "%d\n",((val & 0x03) == 0x03)?0:1);
}

static ssize_t iholden_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var)
    axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x01);
  else
    axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x03);

  return count;
}

static ssize_t ihold_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val,tmp;
  int ihold;
  axp_read(charger->master,AXP22_CHARGE_VBUS, &val);
  tmp = (val) & 0x03;
  switch(tmp){
    case 0: ihold = 900000;break;
    case 1: ihold = 500000;break;
    default: ihold = 0;break;
  }
  return sprintf(buf, "%d\n",ihold);
}

static ssize_t ihold_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var == 900000)
    axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x03);
  else if (var == 500000){
    axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x02);
    axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x01);
  }
  return count;
}

static struct device_attribute axp_charger_attrs[] = {
  AXP_CHG_ATTR(chgen),
  AXP_CHG_ATTR(chgmicrovol),
  AXP_CHG_ATTR(chgintmicrocur),
  AXP_CHG_ATTR(chgendcur),
  AXP_CHG_ATTR(chgpretimemin),
  AXP_CHG_ATTR(chgcsttimemin),
  AXP_CHG_ATTR(adcfreq),
  AXP_CHG_ATTR(vholden),
  AXP_CHG_ATTR(vhold),
  AXP_CHG_ATTR(iholden),
  AXP_CHG_ATTR(ihold),
};
#endif

#if defined CONFIG_HAS_EARLYSUSPEND
static void axp_earlysuspend(struct early_suspend *h)
{
	uint8_t tmp;
	DBG_PSY_MSG("======early suspend=======\n");
	if (axp_shortpress_flag == 1)
	{
	printk(KERN_ERR "[slr]manual suspend");
		manual_suspend_flag = 1;
	}	
#if defined (CONFIG_AXP_CHGCHANGE)
  	early_suspend_flag = 1;
  	if(EARCHGCUR == 0)
  		axp_clr_bits(axp_charger->master,AXP22_CHARGE_CONTROL1,0x80);
  	else
  		axp_set_bits(axp_charger->master,AXP22_CHARGE_CONTROL1,0x80);

    if(EARCHGCUR >= 300000 && EARCHGCUR <= 2550000){
    	tmp = (EARCHGCUR -200001)/150000;
    	axp_update(axp_charger->master, AXP22_CHARGE_CONTROL1, tmp,0x0F);
    }
#endif

}
static void axp_lateresume(struct early_suspend *h)
{
	uint8_t tmp;
	DBG_PSY_MSG("======late resume=======\n");
#if defined (CONFIG_AXP_CHGCHANGE)
	early_suspend_flag = 0;
	if(STACHGCUR == 0)
  		axp_clr_bits(axp_charger->master,AXP22_CHARGE_CONTROL1,0x80);
  else
  		axp_set_bits(axp_charger->master,AXP22_CHARGE_CONTROL1,0x80);

    if(STACHGCUR >= 300000 && STACHGCUR <= 2550000){
        tmp = (STACHGCUR -200001)/150000;
        axp_update(axp_charger->master, AXP22_CHARGE_CONTROL1, tmp,0x0F);
    }
    else if(STACHGCUR < 300000){
    	axp_clr_bits(axp_charger->master, AXP22_CHARGE_CONTROL1,0x0F);
    }
    else{
    	axp_set_bits(axp_charger->master, AXP22_CHARGE_CONTROL1,0x0F);
    }
#endif

}
#endif

#if defined (CONFIG_AXP_DEBUG)
int axp_charger_create_attrs(struct power_supply *psy)
{
  int j,ret;
  for (j = 0; j < ARRAY_SIZE(axp_charger_attrs); j++) {
    ret = device_create_file(psy->dev,
          &axp_charger_attrs[j]);
    if (ret)
      goto sysfs_failed;
  }
    goto succeed;

sysfs_failed:
  while (j--)
    device_remove_file(psy->dev,
         &axp_charger_attrs[j]);
succeed:
  return ret;
}
#endif

static void axp_charging_monitor(struct work_struct *work)
{
	struct axp_charger *charger;
	uint8_t	val,temp_val[4],batt_max_cap_val[3];
	int	pre_rest_vol,pre_bat_curr_dir;
	int coulumb_counter,batt_max_cap;
	charger = container_of(work, struct axp_charger, work.work);
	pre_rest_vol = charger->rest_vol;
	pre_bat_curr_dir = charger->bat_current_direction;
	axp_charger_update_state(charger);
	axp_charger_update(charger);

	axp_read(charger->master, AXP22_CAP,&val);
	charger->rest_vol	= (int)	(val & 0x7F);
	axp_reads(charger->master,0xe2,2,temp_val);
	coulumb_counter = (((temp_val[0] & 0x7f) <<8) + temp_val[1])*1456/1000;
	axp_reads(charger->master,0xe0,2,temp_val);
	batt_max_cap = (((temp_val[0] & 0x7f) <<8) + temp_val[1])*1456/1000;
	/* Avoid the power stay in 100% for a long time. */
	if(coulumb_counter > batt_max_cap){
		batt_max_cap_val[0] = temp_val[0] | (0x1<<7);
		batt_max_cap_val[1] = 0xe3;
		batt_max_cap_val[2] = temp_val[1];
		axp_writes(charger->master, 0xe2,3,batt_max_cap_val);
		DBG_PSY_MSG("Axp22 coulumb_counter111111111111111111 = %d\n",batt_max_cap);
	}
	if(axp_debug){
		DBG_PSY_MSG("charger->ic_temp = %d\n",charger->ic_temp);
		DBG_PSY_MSG("charger->vbat = %d\n",charger->vbat);
		DBG_PSY_MSG("charger->ibat = %d\n",charger->ibat);
		DBG_PSY_MSG("charger->ocv = %d\n",charger->ocv);
		DBG_PSY_MSG("charger->disvbat = %d\n",charger->disvbat);
		DBG_PSY_MSG("charger->disibat = %d\n",charger->disibat);
		DBG_PSY_MSG("charger->rest_vol = %d\n",charger->rest_vol);
		axp_reads(charger->master,0xba,2,temp_val);
		DBG_PSY_MSG("Axp22 Rdc = %d\n",(((temp_val[0] & 0x1f) <<8) + temp_val[1])*10742/10000);
		axp_reads(charger->master,0xe0,2,temp_val);
		DBG_PSY_MSG("Axp22 batt_max_cap = %d\n",(((temp_val[0] & 0x7f) <<8) + temp_val[1])*1456/1000);
		axp_reads(charger->master,0xe2,2,temp_val);
		DBG_PSY_MSG("Axp22 coulumb_counter = %d\n",(((temp_val[0] & 0x7f) <<8) + temp_val[1])*1456/1000);
		axp_read(charger->master,0xb8,temp_val);
		DBG_PSY_MSG("Axp22 REG_B8 = %x\n",temp_val[0]);
		axp_reads(charger->master,0xe4,2,temp_val);
		DBG_PSY_MSG("Axp22 OCV_percentage = %d\n",(temp_val[0] & 0x7f));
		DBG_PSY_MSG("Axp22 Coulumb_percentage = %d\n",(temp_val[1] & 0x7f));
		DBG_PSY_MSG("charger->is_on = %d\n",charger->is_on);
		DBG_PSY_MSG("charger->bat_current_direction = %d\n",charger->bat_current_direction);
		DBG_PSY_MSG("charger->charge_on = %d\n",charger->charge_on);
		DBG_PSY_MSG("charger->ext_valid = %d\n",charger->ext_valid);
		DBG_PSY_MSG("pmu_runtime_chgcur           = %d\n",STACHGCUR);
		DBG_PSY_MSG("pmu_earlysuspend_chgcur   = %d\n",EARCHGCUR);
		DBG_PSY_MSG("pmu_suspend_chgcur        = %d\n",SUSCHGCUR);
		DBG_PSY_MSG("pmu_shutdown_chgcur       = %d\n\n\n",CLSCHGCUR);
//		axp_chip_id_get(chip_id);
	}

	//for test usb detect
#if 0
	val = axp_usb_det();
	if(val)
	{
		printk("axp22 usb or usb adapter can be used!!\n");
	}
	else
	{ 
		printk("axp22 no usb or usb adaper!\n");
	}
#endif	
	/* if battery volume changed, inform uevent */
	if((charger->rest_vol - pre_rest_vol) || (charger->bat_current_direction != pre_bat_curr_dir)){
		if(axp_debug)
			{
				axp_reads(charger->master,0xe2,2,temp_val);
				axp_reads(charger->master,0xe4,2,(temp_val+2));
				printk("%s:battery vol change: %d->%d \n", __func__, pre_rest_vol, charger->rest_vol);
				printk("for test %d %d %d %d %d %d\n",charger->vbat,charger->ocv,charger->ibat,(temp_val[2] & 0x7f),(temp_val[3] & 0x7f),(((temp_val[0] & 0x7f) <<8) + temp_val[1])*1456/1000);
			}
		pre_rest_vol = charger->rest_vol;
		power_supply_changed(&charger->batt);
	}
	/* reschedule for the next time */
	schedule_delayed_work(&charger->work, charger->interval);
}

static void axp_usb(struct work_struct *work)
{
	int var;
	uint8_t tmp,val;
	struct axp_charger *charger;
	
	charger = axp_charger;
	if(axp_debug)
	{
		printk("[axp_usb]axp_usbcurflag = %d\n",axp_usbcurflag);
	}
	if(axp_usbcurflag){
		if(axp_debug)
		{
			printk("set usbcur_pc %d mA\n",USBCURLIMPC);
		}
		if(USBCURLIMPC){
			axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x01);
			var = USBCURLIMPC * 1000;
			if(var >= 900000)
				axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x03);
			else if ((var >= 500000)&& (var < 900000)){
				axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x02);
				axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x01);
			}
			else{
				printk("set usb limit current error,%d mA\n",USBCURLIMPC);	
			} 				
		}
		else//not limit
			axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x03);			
	}else {
		if(axp_debug)
		{
			printk("set usbcur %d mA\n",USBCURLIM);
		}
		if((USBCURLIM) && (USBCURLIMEN)){
			axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x01);
			var = USBCURLIM * 1000;
			if(var >= 900000)
				axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x03);
			else if ((var >= 500000)&& (var < 900000)){
				axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x02);
				axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x01);
			}
			else
				printk("set usb limit current error,%d mA\n",USBCURLIM);	
		}
		else //not limit
			axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x03);
	}
		
	if(axp_usbvolflag){
		if(axp_debug)
		{
			printk("set usbvol_pc %d mV\n",USBVOLLIMPC);
		}
		if(USBVOLLIMPC){
		    axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x40);
		  	var = USBVOLLIMPC * 1000;
		  	if(var >= 4000000 && var <=4700000){
		    	tmp = (var - 4000000)/100000;
		    	axp_read(charger->master, AXP22_CHARGE_VBUS,&val);
		    	val &= 0xC7;
		    	val |= tmp << 3;
		    	axp_write(charger->master, AXP22_CHARGE_VBUS,val);
		  	}
		  	else
		  		printk("set usb limit voltage error,%d mV\n",USBVOLLIMPC);	
		}
		else
		    axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x40);
	}else {
		if(axp_debug)
		{
			printk("set usbvol %d mV\n",USBVOLLIM);
		}
		if((USBVOLLIM) && (USBVOLLIMEN)){
		    axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x40);
		  	var = USBVOLLIM * 1000;
		  	if(var >= 4000000 && var <=4700000){
		    	tmp = (var - 4000000)/100000;
		    	axp_read(charger->master, AXP22_CHARGE_VBUS,&val);
		    	val &= 0xC7;
		    	val |= tmp << 3;
		    	axp_write(charger->master, AXP22_CHARGE_VBUS,val);
		  	}
		  	else
		  		printk("set usb limit voltage error,%d mV\n",USBVOLLIM);	
		}
		else
		    axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x40);
	}
	schedule_delayed_work(&usbwork, msecs_to_jiffies(5* 1000));
}

int rk30_adc_battery_get_bat_vol(void)
{
	printk("%s, enter...\n", __func__);
	if(!axp_charger) {
		printk("%s,axp_charger is NULL!\n", __func__);
		return -EINVAL;
	}
	printk("%s, ---axp_charger->vbat: %d---\n", __func__, axp_charger->vbat);
	return (axp_charger->vbat >= SHUTDOWNVOL) ? 0:1;
}

void rk30_adc_ebc_battery_check(void)
{
	printk("%s, enter...\n", __func__);
	if(!axp_charger) {
		printk("%s,axp_charger is NULL!\n", __func__);
		return;
	}

	if (axp_charger->usb_det == 0) { /*no charger*/
		if (rk30_adc_battery_get_bat_vol() > 0 || axp_charger->rest_vol < 3) {
			printk("%s, low power! [no-charger]\n", __func__);
			/* power off */
			while (1) {
				if (pm_power_off)
					pm_power_off();
				mdelay(100);
			}
		}
	} else {
		if (rk30_adc_battery_get_bat_vol() > 0 || axp_charger->rest_vol < 3) {
			printk("%s, low power! [with-charger]\n", __func__);
		}
	}
}

//boyue slr add for exporting the axp dev 
struct device* my_axp_dev;
EXPORT_SYMBOL_GPL(my_axp_dev);
struct device *get_my_axp_dev(void)
{
	return my_axp_dev;
}
EXPORT_SYMBOL_GPL(get_my_axp_dev);

static int axp_battery_probe(struct platform_device *pdev)
{
  struct axp_charger *charger;
  struct axp_supply_init_data *pdata = pdev->dev.platform_data;
  int ret,var,tmp;
  uint8_t val2,val;
  uint8_t ocv_cap[63];
  int Cur_CoulombCounter,rdc;
  
  powerkeydev = input_allocate_device();
  if (!powerkeydev) {
    kfree(powerkeydev);
    return -ENODEV;
  }

  powerkeydev->name = pdev->name;
  powerkeydev->phys = "m1kbd/input2";
  powerkeydev->id.bustype = BUS_HOST;
  powerkeydev->id.vendor = 0x0001;
  powerkeydev->id.product = 0x0001;
  powerkeydev->id.version = 0x0100;
  powerkeydev->open = NULL;
  powerkeydev->close = NULL;
  powerkeydev->dev.parent = &pdev->dev;

  set_bit(EV_KEY, powerkeydev->evbit);
  set_bit(EV_REL, powerkeydev->evbit);
  //set_bit(EV_REP, powerkeydev->evbit);
  set_bit(KEY_POWER, powerkeydev->keybit);

  //input_set_capability(powerkeydev, EV_KEY, KEY_WAKEUP);

  ret = input_register_device(powerkeydev);
  if(ret) {
    printk("Unable to Register the power key\n");
    }

  if (pdata == NULL)
    return -EINVAL;

  printk("axp charger not limit now\n");
  if (pdata->chgcur > 2550000 ||
      pdata->chgvol < 4100000 ||
      pdata->chgvol > 4240000){
        printk("charger milliamp is too high or target voltage is over range\n");
        return -EINVAL;
    }

  if (pdata->chgpretime < 40 || pdata->chgpretime >70 ||
    pdata->chgcsttime < 360 || pdata->chgcsttime > 720){
            printk("prechaging time or constant current charging time is over range\n");
        return -EINVAL;
  }

  charger = kzalloc(sizeof(*charger), GFP_KERNEL);
  if (charger == NULL)
    return -ENOMEM;

  charger->master = pdev->dev.parent;
//boyue slr add for exporting the axp dev  
  my_axp_dev = pdev->dev.parent;

  charger->chgcur      = pdata->chgcur;
  charger->chgvol     = pdata->chgvol;
  charger->chgend           = pdata->chgend;
  charger->sample_time          = pdata->sample_time;
  charger->chgen                   = pdata->chgen;
  charger->chgpretime      = pdata->chgpretime;
  charger->chgcsttime = pdata->chgcsttime;
  charger->battery_info         = pdata->battery_info;
  charger->disvbat			= 0;
  charger->disibat			= 0;

  ret = axp_battery_first_init(charger);
  if (ret)
    goto err_charger_init;

//  printk("add axp_battery_event to notifier[%2x]\n", axp_battery_event);
  charger->nb.notifier_call = axp_battery_event;
  ret = axp_register_notifier(charger->master, &charger->nb, AXP22_NOTIFIER_ON);
  if (ret)
    goto err_notifier;

  axp_battery_setup_psy(charger);
  ret = power_supply_register(&pdev->dev, &charger->batt);
  if (ret)
    goto err_ps_register;

	axp_read(charger->master,AXP22_CHARGE_STATUS,&val);
	if(!((val >> 1) & 0x01)){
  	ret = power_supply_register(&pdev->dev, &charger->ac);
  	if (ret){
    	power_supply_unregister(&charger->batt);
    	goto err_ps_register;
  	}
  }

  ret = power_supply_register(&pdev->dev, &charger->usb);
  if (ret){
    power_supply_unregister(&charger->ac);
    power_supply_unregister(&charger->batt);
    goto err_ps_register;
  }

#if defined (CONFIG_AXP_DEBUG)
  ret = axp_charger_create_attrs(&charger->batt);
  if(ret){
  	printk("cat notaxp_charger_create_attrs!!!===\n ");
    return ret;
  }
#endif  

  platform_set_drvdata(pdev, charger);
  
  /* usb voltage limit */
  if((USBVOLLIM) && (USBVOLLIMEN)){
    axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x40);
  	var = USBVOLLIM * 1000;
  	if(var >= 4000000 && var <=4700000){
    	tmp = (var - 4000000)/100000;
    	axp_read(charger->master, AXP22_CHARGE_VBUS,&val);
    	val &= 0xC7;
    	val |= tmp << 3;
    	axp_write(charger->master, AXP22_CHARGE_VBUS,val);
  	}
  }
  else
    axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x40);
    
	/*usb current limit*/
  if((USBCURLIM) && (USBCURLIMEN)){
    axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x02);
    var = USBCURLIM * 1000;
  	if(var == 900000)
    	axp_clr_bits(charger->master, AXP22_CHARGE_VBUS, 0x03);
  	else if (var == 500000){
    	axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x01);
  	}
  }
  else
    axp_set_bits(charger->master, AXP22_CHARGE_VBUS, 0x03);
      

  // set lowe power warning/shutdown level
  axp_write(charger->master, AXP22_WARNING_LEVEL,((BATLOWLV1-5) << 4)+BATLOWLV2);

  ocv_cap[0]  = OCVREG0;
  ocv_cap[1]  = 0xC1;
  ocv_cap[2]  = OCVREG1;
  ocv_cap[3]  = 0xC2;
  ocv_cap[4]  = OCVREG2;
  ocv_cap[5]  = 0xC3;
  ocv_cap[6]  = OCVREG3;
  ocv_cap[7]  = 0xC4;
  ocv_cap[8]  = OCVREG4;
  ocv_cap[9]  = 0xC5;
  ocv_cap[10] = OCVREG5;
  ocv_cap[11] = 0xC6;
  ocv_cap[12] = OCVREG6;
  ocv_cap[13] = 0xC7;
  ocv_cap[14] = OCVREG7;
  ocv_cap[15] = 0xC8;
  ocv_cap[16] = OCVREG8;
  ocv_cap[17] = 0xC9;
  ocv_cap[18] = OCVREG9;
  ocv_cap[19] = 0xCA;
  ocv_cap[20] = OCVREGA;
  ocv_cap[21] = 0xCB;
  ocv_cap[22] = OCVREGB;
  ocv_cap[23] = 0xCC;
  ocv_cap[24] = OCVREGC;
  ocv_cap[25] = 0xCD;
  ocv_cap[26] = OCVREGD;
  ocv_cap[27] = 0xCE;
  ocv_cap[28] = OCVREGE;
  ocv_cap[29] = 0xCF;
  ocv_cap[30] = OCVREGF;
  ocv_cap[31] = 0xD0;
  ocv_cap[32] = OCVREG10;
  ocv_cap[33] = 0xD1;
  ocv_cap[34] = OCVREG11;
  ocv_cap[35] = 0xD2;
  ocv_cap[36] = OCVREG12;
  ocv_cap[37] = 0xD3;
  ocv_cap[38] = OCVREG13;
  ocv_cap[39] = 0xD4;
  ocv_cap[40] = OCVREG14;
  ocv_cap[41] = 0xD5;
  ocv_cap[42] = OCVREG15;
  ocv_cap[43] = 0xD6;
  ocv_cap[44] = OCVREG16;
  ocv_cap[45] = 0xD7;
  ocv_cap[46] = OCVREG17;
  ocv_cap[47] = 0xD8;
  ocv_cap[48] = OCVREG18;
  ocv_cap[49] = 0xD9;
  ocv_cap[50] = OCVREG19;
  ocv_cap[51] = 0xDA;
  ocv_cap[52] = OCVREG1A;
  ocv_cap[53] = 0xDB;
  ocv_cap[54] = OCVREG1B;
  ocv_cap[55] = 0xDC;
  ocv_cap[56] = OCVREG1C;
  ocv_cap[57] = 0xDD;
  ocv_cap[58] = OCVREG1D;
  ocv_cap[59] = 0xDE;
  ocv_cap[60] = OCVREG1E;
  ocv_cap[61] = 0xDF;
  ocv_cap[62] = OCVREG1F;
  axp_writes(charger->master, 0xC0,31,ocv_cap);
  axp_writes(charger->master, 0xD0,31,ocv_cap+32);
	/* pok open time set */
	axp_read(charger->master,AXP22_POK_SET,&val);
	if (PEKOPEN < 1000)
		val &= 0x3f;
	else if(PEKOPEN < 2000){
		val &= 0x3f;
		val |= 0x40;
	}
	else if(PEKOPEN < 3000){
		val &= 0x3f;
		val |= 0x80;
	}
	else {
		val &= 0x3f;
		val |= 0xc0;
	}
	axp_write(charger->master,AXP22_POK_SET,val);

	/* pok long time set*/
	if(PEKLONG < 1000)
		tmp = 1000;
	else if(PEKLONG > 2500)
		tmp = 2500;
	else
		tmp = PEKLONG;
	axp_read(charger->master,AXP22_POK_SET,&val);
	val &= 0xcf;
	val |= (((tmp - 1000) / 500) << 4);
	axp_write(charger->master,AXP22_POK_SET,val);

	/* pek offlevel poweroff en set*/
	if(PEKOFFEN)
	{
			tmp = 1;
	}
	else
	{
			tmp = 0;			
	}
	axp_read(charger->master,AXP22_POK_SET,&val);
	val &= 0xf7;
	val |= (tmp << 3);
	axp_write(charger->master,AXP22_POK_SET,val);
	
	/*Init offlevel restart or not */
	if(PEKOFFRESTART)
	{
			axp_set_bits(charger->master,AXP22_POK_SET,0x04); //restart
	}
	else
	{
			axp_clr_bits(charger->master,AXP22_POK_SET,0x04); //not restart
	}

	/* pek delay set */
	axp_read(charger->master,AXP22_OFF_CTL,&val);
	val &= 0xfc;
	val |= ((PEKDELAY / 8) - 1);
	axp_write(charger->master,AXP22_OFF_CTL,val);

	/* pek offlevel time set */
	if(PEKOFF < 4000)
		tmp = 4000;
	else if(PEKOFF > 10000)
		tmp =10000;
	else
		tmp = PEKOFF;
	tmp = (tmp - 4000) / 2000 ;
	axp_read(charger->master,AXP22_POK_SET,&val);
	val &= 0xfc;
	val |= tmp ;
	axp_write(charger->master,AXP22_POK_SET,val);
	/*Init 16's Reset PMU en */
	if(PMURESET)
	{
		axp_set_bits(charger->master,0x8F,0x08); //enable
	}
	else
	{
		axp_clr_bits(charger->master,0x8F,0x08); //disable
	}
		
	/*Init IRQ wakeup en*/
	if(IRQWAKEUP)
	{
		axp_set_bits(charger->master,0x8F,0x80); //enable
	}
	else
	{
		axp_clr_bits(charger->master,0x8F,0x80); //disable
	}
		
	/*Init N_VBUSEN status*/
	if(VBUSEN)
	{
		axp_set_bits(charger->master,0x8F,0x10); //output
	}
	else
	{
		printk(KERN_ERR "[slr]N_VBUSEN io on axp is set to output\n");	
		axp_clr_bits(charger->master,0x8F,0x10); //output
	//slr add, IPSOUT does not select VBUS 
	//	axp_set_bits(charger->master,0x30,0x05); //output 1
	}
		
	/*Init InShort status*/
	if(VBUSACINSHORT)
	{
		axp_set_bits(charger->master,0x8F,0x60); //InShort
	}
	else
	{
		axp_clr_bits(charger->master,0x8F,0x60); //auto detect
	}
		
	/*Init CHGLED function*/
	if(CHGLEDFUN)
	{
		axp_set_bits(charger->master,0x32,0x08); //control by charger
	}
	else
	{
		axp_clr_bits(charger->master,0x32,0x08); //drive MOTO
	}
		
	/*set CHGLED Indication Type*/
	if(CHGLEDTYPE)
	{
		axp_set_bits(charger->master,0x45,0x10); //Type A
	}
	else
	{
		axp_clr_bits(charger->master,0x45,0x10); //Type B
	}
		
	/*Init PMU Over Temperature protection*/
	if(OTPOFFEN)
	{
		axp_set_bits(charger->master,0x8f,0x04); //enable
	}
	else
	{
		axp_clr_bits(charger->master,0x8f,0x04); //disable
	}

	/*Init battery capacity correct function*/
	if(BATCAPCORRENT)
	{
		axp_set_bits(charger->master,0xb8,0x20); //enable
	}
	else
	{
		axp_clr_bits(charger->master,0xb8,0x20); //disable
	}
	/* Init battery regulator enable or not when charge finish*/
	if(BATREGUEN)
	{
		axp_set_bits(charger->master,0x34,0x20); //enable
	}
	else
	{
		axp_clr_bits(charger->master,0x34,0x20); //disable
	}
 
	if(!BATDET)
		axp_clr_bits(charger->master,AXP22_PDBC,0x40);
	else
		axp_set_bits(charger->master,AXP22_PDBC,0x40);
  	
/* RDC initial */
	axp_read(charger->master, AXP22_RDC0,&val2);
	if((BATRDC) && (!(val2 & 0x40)))		//如果配置电池内阻，则手动配置
	{
		rdc = (BATRDC * 10000 + 5371) / 10742;
		axp_write(charger->master, AXP22_RDC0, ((rdc >> 8) & 0x1F)|0x80);
		axp_write(charger->master,AXP22_RDC1,rdc & 0x00FF);
	}

//probe 时初始化RDC，使其提前计算正确的OCV，然后在此处启动计量系统
	axp_read(charger->master,AXP22_BATCAP0,&val2);
	if((BATCAP) && (!(val2 & 0x80)))
	{
		Cur_CoulombCounter = BATCAP * 1000 / 1456;
		axp_write(charger->master, AXP22_BATCAP0, ((Cur_CoulombCounter >> 8) | 0x80));
		axp_write(charger->master,AXP22_BATCAP1,Cur_CoulombCounter & 0x00FF);		
	}
	else if(!BATCAP)
	{
		axp_write(charger->master, AXP22_BATCAP0, 0x00);
		axp_write(charger->master,AXP22_BATCAP1,0x00);
	}
  
  axp_charger_update_state((struct axp_charger *)charger);

  axp_read(charger->master, AXP22_CAP,&val2);
	charger->rest_vol = (int) (val2 & 0x7F);

  printk("now_rest_vol = %d\n",(val2 & 0x7F));
  charger->interval = msecs_to_jiffies(10 * 1000);
  INIT_DELAYED_WORK(&charger->work, axp_charging_monitor);
  schedule_delayed_work(&charger->work, charger->interval);

	/* set usb cur-vol limit*/
	INIT_DELAYED_WORK(&usbwork, axp_usb);
	schedule_delayed_work(&usbwork, msecs_to_jiffies(7 * 1000));
	axp_charger_update(charger);
	/*给局部变量赋值*/
	axp_charger = charger;

	rk30_adc_ebc_battery_check();

	wake_lock_init(&charger->axp_lock, WAKE_LOCK_SUSPEND, "axp_lock");
#ifdef CONFIG_HAS_EARLYSUSPEND
    axp_early_suspend.suspend = axp_earlysuspend;
    axp_early_suspend.resume = axp_lateresume;
    axp_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 2;
    register_early_suspend(&axp_early_suspend);
#endif
	/* 调试接口注册 */
	class_register(&axppower_class);

    return ret;

err_ps_register:
  axp_unregister_notifier(charger->master, &charger->nb, AXP22_NOTIFIER_ON);

err_notifier:
  cancel_delayed_work_sync(&charger->work);

err_charger_init:
  kfree(charger);
  input_unregister_device(powerkeydev);
  kfree(powerkeydev);
  return ret;
}

static int axp_battery_remove(struct platform_device *dev)
{
    struct axp_charger *charger = platform_get_drvdata(dev);

    if(main_task){
        kthread_stop(main_task);
        main_task = NULL;
    }

    axp_unregister_notifier(charger->master, &charger->nb, AXP22_NOTIFIER_ON);
    cancel_delayed_work_sync(&charger->work);
    power_supply_unregister(&charger->usb);
    power_supply_unregister(&charger->ac);
    power_supply_unregister(&charger->batt);

    kfree(charger);
    input_unregister_device(powerkeydev);
    kfree(powerkeydev);

    return 0;
}


static int axp22_suspend(struct platform_device *dev, pm_message_t state)
{
    uint8_t irq_w[9];
    uint8_t tmp;

    struct axp_charger *charger = platform_get_drvdata(dev);

	DBG_PSY_MSG("%s, entor...\n", __func__);
	cancel_delayed_work_sync(&charger->work);
	cancel_delayed_work_sync(&usbwork);

    /*clear all irqs events*/
    irq_w[0] = 0xff;
    irq_w[1] = AXP22_INTSTS2;
    irq_w[2] = 0xff;
    irq_w[3] = AXP22_INTSTS3;
    irq_w[4] = 0xff;
    irq_w[5] = AXP22_INTSTS4;
    irq_w[6] = 0xff;
    irq_w[7] = AXP22_INTSTS5;
    irq_w[8] = 0xff;
    axp_writes(charger->master, AXP22_INTSTS1, 9, irq_w);

    /* close all irqs*/
	axp_unregister_notifier(charger->master, &charger->nb, AXP22_NOTIFIER_ON);

#if defined (CONFIG_AXP_CHGCHANGE)
    if(SUSCHGCUR == 0)
        axp_clr_bits(charger->master,AXP22_CHARGE_CONTROL1,0x80);
    else
        axp_set_bits(charger->master,AXP22_CHARGE_CONTROL1,0x80);

    printk("pmu_suspend_chgcur = %d\n", SUSCHGCUR);

    if(SUSCHGCUR >= 300000 && SUSCHGCUR <= 2550000){
        tmp = (SUSCHGCUR -200001)/150000;
        charger->chgcur = tmp *150000 + 300000;
        axp_update(charger->master, AXP22_CHARGE_CONTROL1, tmp,0x0F);
    }
#endif

    return 0;
}

static int axp22_resume(struct platform_device *dev)
{
    struct axp_charger *charger = platform_get_drvdata(dev);

    int pre_rest_vol;
    uint8_t val,tmp;
    DBG_PSY_MSG("%s, entor...\n", __func__);
	
    /*wakeup IQR notifier work sequence*/
    axp_register_notifier(charger->master, &charger->nb, AXP22_NOTIFIER_ON);

    axp_charger_update_state(charger);

    pre_rest_vol = charger->rest_vol;

    axp_read(charger->master, AXP22_CAP,&val);
    charger->rest_vol = val & 0x7f;

    if(charger->rest_vol - pre_rest_vol){
    	printk("%s:battery vol change: %d->%d \n", __func__, pre_rest_vol, charger->rest_vol);
    	pre_rest_vol = charger->rest_vol;
    	axp_write(charger->master,AXP22_DATA_BUFFER1,charger->rest_vol | 0x80);
    	power_supply_changed(&charger->batt);
    }

#if defined (CONFIG_AXP_CHGCHANGE)
    if(STACHGCUR == 0)
    	axp_clr_bits(charger->master,AXP22_CHARGE_CONTROL1,0x80);
    else
    	axp_set_bits(charger->master,AXP22_CHARGE_CONTROL1,0x80);

    printk("pmu_runtime_chgcur = %d\n", STACHGCUR);

    if(STACHGCUR >= 300000 && STACHGCUR <= 2550000){
        tmp = (STACHGCUR -200001)/150000;
        charger->chgcur = tmp *150000 + 300000;
        axp_update(charger->master, AXP22_CHARGE_CONTROL1, tmp,0x0F);
    }else if(STACHGCUR < 300000){
    	axp_clr_bits(axp_charger->master, AXP22_CHARGE_CONTROL1,0x0F);
    }
    else{
    	axp_set_bits(axp_charger->master, AXP22_CHARGE_CONTROL1,0x0F);
    }
#endif

    charger->disvbat = 0;
    charger->disibat = 0;
    schedule_delayed_work(&charger->work, msecs_to_jiffies(2 * 1000));
    schedule_delayed_work(&usbwork, msecs_to_jiffies(2 * 1000));
    return 0;
}

static void axp22_shutdown(struct platform_device *dev)
{
    uint8_t tmp;
    struct axp_charger *charger = platform_get_drvdata(dev);
    
    cancel_delayed_work_sync(&charger->work);

#if defined (CONFIG_AXP_CHGCHANGE)
    if(CLSCHGCUR == 0)
        axp_clr_bits(charger->master,AXP22_CHARGE_CONTROL1,0x80);
    else
        axp_set_bits(charger->master,AXP22_CHARGE_CONTROL1,0x80);

    printk("pmu_shutdown_chgcur = %d\n", CLSCHGCUR);

    if(CLSCHGCUR >= 300000 && CLSCHGCUR <= 2550000){
    	tmp = (CLSCHGCUR -200001)/150000;
    	charger->chgcur = tmp *150000 + 300000;
    	axp_update(charger->master, AXP22_CHARGE_CONTROL1, tmp, 0x0F);
    }
#endif
}

static struct platform_driver axp_battery_driver = {
  .driver = {
    .name = "axp22-supplyer",
    .owner  = THIS_MODULE,
  },
  .probe = axp_battery_probe,
  .remove = axp_battery_remove,
  .suspend = axp22_suspend,
  .resume = axp22_resume,
  .shutdown = axp22_shutdown,
};

static int axp_battery_init(void)
{
    int ret =0;
    ret = platform_driver_register(&axp_battery_driver);
    return ret;
}

static void axp_battery_exit(void)
{
    platform_driver_unregister(&axp_battery_driver);
}

subsys_initcall(axp_battery_init);
module_exit(axp_battery_exit);

MODULE_DESCRIPTION("AXP22 battery charger driver");
MODULE_AUTHOR("Weijin Zhong");
MODULE_LICENSE("GPL");
