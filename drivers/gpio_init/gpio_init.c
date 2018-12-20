#include <linux/module.h>
#include <linux/kernel.h>
#include <mach/gpio.h>


#if defined(CONFIG_MACH_RK3026_T63)

static int close_led(void)
{
#define BLUE_LED_GPIO RK30_PIN2_PB5
#define GREEN_LED_GPIO RK30_PIN2_PB6

	int ret = 0;
	ret = gpio_request(BLUE_LED_GPIO, "blue_led");
	if(ret)
	{
		gpio_free(BLUE_LED_GPIO);
		ret = gpio_request(BLUE_LED_GPIO, "blue_led");
//		return -EIO;
	}
	if (ret)
	{
		printk("%s:request blue led gpio error!\n", __FILE__);
	}
	else
	{
		gpio_pull_updown(BLUE_LED_GPIO, GPIOPullDown);
		gpio_direction_output(BLUE_LED_GPIO, GPIO_LOW);
		printk("%s:init blue led gpio, turn off blue led \n", __FILE__);
	}

	
	ret = gpio_request(GREEN_LED_GPIO, "green_led");
	if(ret)
	{
		gpio_free(GREEN_LED_GPIO);
		ret = gpio_request(GREEN_LED_GPIO, "green_led");
//		return -EIO;
	}
	if (ret)
	{
		printk("%s:request green led gpio error!\n", __FILE__);
	}
	else
	{
		gpio_pull_updown(GREEN_LED_GPIO, GPIOPullDown);
		gpio_direction_output(GREEN_LED_GPIO, GPIO_LOW);
		printk("%s:init green led gpio, turn off green led \n", __FILE__);
	}
	return ret;
}


static int shutdown_otg_drv(void)
{
#define OTG_DRV_GPIO RK30_PIN2_PB4

	int ret = 0;

	ret = gpio_request(OTG_DRV_GPIO, "otg_drv");
	if(unlikely(ret)) {
		gpio_free(OTG_DRV_GPIO);
		printk(KERN_ERR "%s: failed to request otg_drv gpio\n", __FILE__);
		return ret;
	}
	else
	{
		gpio_pull_updown(OTG_DRV_GPIO, GPIOPullDown);
		gpio_direction_output(OTG_DRV_GPIO, GPIO_LOW);
		printk(KERN_ERR "[slr] pull down the OTG_DRV gpio to keep safe when booting\n");
	}
	return ret;
}
#endif




static int __init gpio_init(void)
{
	printk("enter %s\n", __func__);
#if defined(CONFIG_MACH_RK3026_T63)	
	//slr
	//some program change some GPIO level but can't exactly find the place
	//the below 2 functions were added for solving problems for now
	close_led();
	shutdown_otg_drv();
#endif

	return 0;
}

static void __exit gpio_exit(void)
{
	printk("enter %s\n", __func__);

}


late_initcall(gpio_init);
module_exit(gpio_exit);
MODULE_AUTHOR("shiliran@boyue.com");
MODULE_DESCRIPTION("some spio have to be inited late");
MODULE_LICENSE("GPL");

