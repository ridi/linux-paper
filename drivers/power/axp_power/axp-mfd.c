/*
 * Base driver for X-Powers AXP
 *
 * Copyright (C) 2013 X-Powers, Ltd.
 *  Zhang Donglu <zhangdonglu@x-powers.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <mach/system.h>
#include <mach/gpio.h>
#include <linux/earlysuspend.h>

//#include "axp-cfg.h"
#include "axp22-mfd.h"

static struct early_suspend axp22_suspend;
void axp_mfd_suspend(struct early_suspend *h);
void axp_mfd_resume(struct early_suspend *h);
static void axp_mfd_irq_work(struct work_struct *work)
{
	struct axp_mfd_chip *chip = container_of(work, struct axp_mfd_chip, irq_work);
	uint64_t irqs = 0;
	
	while (1) {
		if (chip->ops->read_irqs(chip, &irqs)){
			printk("read irq fail\n");
			break;
		}
		irqs &= chip->irqs_enabled;
		if (irqs == 0){
			break;
		}
		
		if(irqs > 0xffffffff){
			blocking_notifier_call_chain(&chip->notifier_list, (uint32_t)(irqs>>32), (void *)1);
		}
		else{
			blocking_notifier_call_chain(&chip->notifier_list, (uint32_t)irqs, (void *)0);
		}
	}
	enable_irq(chip->client->irq);
}

static irqreturn_t axp_mfd_irq_handler(int irq, void *data)
{
	struct axp_mfd_chip *chip = data;
	disable_irq_nosync(irq);
	(void)schedule_work(&chip->irq_work);

	return IRQ_HANDLED;
}

static struct axp_mfd_chip_ops axp_mfd_ops[] = {
	[0] = {
		.init_chip    = axp22_init_chip,
		.enable_irqs  = axp22_enable_irqs,
		.disable_irqs = axp22_disable_irqs,
		.read_irqs    = axp22_read_irqs,
	},
};

static const struct i2c_device_id axp_mfd_id_table[] = {
	{ "axp22_mfd", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, axp_mfd_id_table);

int axp_mfd_create_attrs(struct axp_mfd_chip *chip)
{
	int j,ret;
	if (chip->type ==  AXP22){
		for (j = 0; j < ARRAY_SIZE(axp22_mfd_attrs); j++) {
			ret = device_create_file(chip->dev,&axp22_mfd_attrs[j]);
			if (ret)
			goto sysfs_failed;
		}
	}
	else
		ret = 0;
	goto succeed;

sysfs_failed:
	while (j--)
		device_remove_file(chip->dev,&axp22_mfd_attrs[j]);
succeed:
	return ret;
}

static int __remove_subdev(struct device *dev, void *unused)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

static int axp_mfd_remove_subdevs(struct axp_mfd_chip *chip)
{
	return device_for_each_child(chip->dev, NULL, __remove_subdev);
}

static int __devinit axp_mfd_add_subdevs(struct axp_mfd_chip *chip,
					struct axp_platform_data *pdata)
{
	struct axp_funcdev_info *regl_dev;
	struct axp_funcdev_info *sply_dev;
	struct axp_funcdev_info *gpio_dev;
	struct platform_device *pdev;
	int i, ret = 0;

	/* register for regultors */
	for (i = 0; i < pdata->num_regl_devs; i++) {
		regl_dev = &pdata->regl_devs[i];
		pdev = platform_device_alloc(regl_dev->name, regl_dev->id);
		pdev->dev.parent = chip->dev;
		pdev->dev.platform_data = regl_dev->platform_data;
		ret = platform_device_add(pdev);
		if (ret)
			goto failed;
	}

	/* register for power supply */
	for (i = 0; i < pdata->num_sply_devs; i++) {
	sply_dev = &pdata->sply_devs[i];
	pdev = platform_device_alloc(sply_dev->name, sply_dev->id);
	pdev->dev.parent = chip->dev;
	pdev->dev.platform_data = sply_dev->platform_data;
	ret = platform_device_add(pdev);
	if (ret)
		goto failed;

	}

	/* register for gpio */
	for (i = 0; i < pdata->num_gpio_devs; i++) {
	gpio_dev = &pdata->gpio_devs[i];
	pdev = platform_device_alloc(gpio_dev->name, gpio_dev->id);
	pdev->dev.parent = chip->dev;
	pdev->dev.platform_data = gpio_dev->platform_data;
	ret = platform_device_add(pdev);
	if (ret)
		goto failed;
	}

	return 0;

failed:
	axp_mfd_remove_subdevs(chip);
	return ret;
}

static void axp_power_off(void)
{
	uint8_t val;

#if defined (CONFIG_KP_AXP22)
	if(SHUTDOWNVOL >= 2600 && SHUTDOWNVOL <= 3300){
		if (SHUTDOWNVOL > 3200){
			val = 0x7;
		}
		else if (SHUTDOWNVOL > 3100){
			val = 0x6;
		}
		else if (SHUTDOWNVOL > 3000){
			val = 0x5;
		}
		else if (SHUTDOWNVOL > 2900){
			val = 0x4;
		}
		else if (SHUTDOWNVOL > 2800){
			val = 0x3;
		}
		else if (SHUTDOWNVOL > 2700){
			val = 0x2;
		}
		else if (SHUTDOWNVOL > 2600){
			val = 0x1;
		}
		else
			val = 0x0;

		axp_update(&axp->dev, AXP22_VOFF_SET, val, 0x7);
	}
	val = 0xff;
    printk("[axp] send power-off command!\n");
    mdelay(20);
    if(POWER_START != 1){
		axp_read(&axp->dev, AXP22_STATUS, &val);
		if(val & 0xF0){
	    	axp_read(&axp->dev, AXP22_MODE_CHGSTATUS, &val);
	    	if(val & 0x20){
            	printk("[axp] set flag!\n");
	        	axp_write(&axp->dev, AXP22_BUFFERC, 0x0f);
            	mdelay(20);
		    	printk("[axp] reboot!\n");
		    	arch_reset(0,NULL);
		    	printk("[axp] warning!!! arch can't ,reboot, maybe some error happend!\n");
	    	}
		}
	}
    axp_write(&axp->dev, AXP22_BUFFERC, 0x00);
    mdelay(20);
    axp_set_bits(&axp->dev, AXP22_OFF_CTL, 0x80);
    mdelay(20);
    printk("[axp] warning!!! axp can't power-off, maybe some error happend!\n");

#endif
}

static int __devinit axp_mfd_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct axp_platform_data *pdata = client->dev.platform_data;
	struct axp_mfd_chip *chip;
	int ret;

	chip = kzalloc(sizeof(struct axp_mfd_chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;

	axp = client;

	chip->client = client;
	chip->dev = &client->dev;
	chip->ops = &axp_mfd_ops[id->driver_data];

	mutex_init(&chip->lock);
	INIT_WORK(&chip->irq_work, axp_mfd_irq_work);
	BLOCKING_INIT_NOTIFIER_HEAD(&chip->notifier_list);

	i2c_set_clientdata(client, chip);

	ret = chip->ops->init_chip(chip);
	if (ret)
		goto out_free_chip;

	if(gpio_request(client->irq, "pmu_int")) {
		dev_err(&client->dev, "pmu_int gpio request fail\n");
		gpio_free(client->irq);
		goto out_free_chip;
	}
	gpio_pull_updown(client->irq, PullDisable);

	client->irq = gpio_to_irq(client->irq);
	ret = request_irq(client->irq, axp_mfd_irq_handler,
		IRQF_DISABLED|IRQF_TRIGGER_FALLING, "axp_mfd", chip);
  	if (ret) {
  		dev_err(&client->dev, "failed to request irq %d\n",
  				client->irq);
  		goto out_free_chip;
  	}

	ret = axp_mfd_add_subdevs(chip, pdata);
	if (ret)
		goto out_free_irq;

	/* PM hookup */
	if(!pm_power_off)
		pm_power_off = axp_power_off;

	ret = axp_mfd_create_attrs(chip);
	if(ret){
		return ret;
	}
	device_init_wakeup(&client->dev, 1);
#ifdef CONFIG_HAS_EARLYSUSPEND
	axp22_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	axp22_suspend.suspend = axp_mfd_suspend;
	axp22_suspend.resume = axp_mfd_resume;
	register_early_suspend(&axp22_suspend);
#endif
	return 0;

out_free_irq:
	free_irq(client->irq, chip);

out_free_chip:
	i2c_set_clientdata(client, NULL);
	kfree(chip);

	return ret;
}

void axp_mfd_resume(struct early_suspend *h)
{
	if(device_may_wakeup(&axp->dev)){
		disable_irq_wake(axp->irq);
	}

	/*enable_irq(axp->irq);*/
	return;
}

void axp_mfd_suspend(struct early_suspend *h)
{	
	if(device_may_wakeup(&axp->dev)){
		enable_irq_wake(axp->irq);
	}

	/*disable_irq(axp->irq);*/
	return;
}

static int __devexit axp_mfd_remove(struct i2c_client *client)
{
	struct axp_mfd_chip *chip = i2c_get_clientdata(client);

	pm_power_off = NULL;
	axp = NULL;

	axp_mfd_remove_subdevs(chip);
	kfree(chip);
	return 0;
}

static struct i2c_driver axp_mfd_driver = {
	.driver	= {
		.name	= "axp_mfd",
		.owner	= THIS_MODULE,
	},
	.probe		= axp_mfd_probe,
	.remove		= __devexit_p(axp_mfd_remove),
	.id_table	= axp_mfd_id_table,
};

static int __init axp_mfd_init(void)
{
	return i2c_add_driver(&axp_mfd_driver);
}
subsys_initcall(axp_mfd_init);

static void __exit axp_mfd_exit(void)
{
	i2c_del_driver(&axp_mfd_driver);
}
module_exit(axp_mfd_exit);

MODULE_DESCRIPTION("MFD Driver for X-Powers AXP PMIC");
MODULE_AUTHOR("Weijin Zhong X-POWERS");
MODULE_LICENSE("GPL");
