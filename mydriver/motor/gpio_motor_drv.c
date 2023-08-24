#include <linux/module.h>
#include <linux/poll.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <asm/current.h>
#include <linux/delay.h>
#include <linux/wait.h>

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_motor_class;
static struct gpio_desc *motor_a;
static struct gpio_desc *motor_b;
static struct gpio_desc *motor_c;
static struct gpio_desc *motor_d;

static int  S_CW[8]=  {0x2,0x3,0x1,0x9,0x8,0xc,0x4,0x6};
static int g_index = 0;

void set_pins_for_motor(int index)
{
	gpiod_set_value(motor_a,S_CW[index] & (1<<0)?1:0);
	gpiod_set_value(motor_b,S_CW[index] & (1<<1)?1:0);
	gpiod_set_value(motor_c,S_CW[index] & (1<<2)?1:0);
	gpiod_set_value(motor_d,S_CW[index] & (1<<3)?1:0);

}

/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t gpio_key_drv_write (struct file *file,const char __user *buf, size_t size, loff_t *offset)
{
	int ker_buf;
	int err;
	int step;

	if(size != 4)
		return -EINVAL;

	err = copy_from_user(&ker_buf,buf,size);

	if(ker_buf > 0)
	{
		for(step = 0; step < ker_buf;step++)
		{
			set_pins_for_motor(g_index);
			mdelay(1);
			g_index--;
			if(g_index == -1)
				g_index = 7;
		}
	}
	else
	{
		ker_buf = 0 - ker_buf;
		for(step = 0; step < ker_buf;step++)
		{
			set_pins_for_motor(g_index);
			mdelay(1);
			g_index++;
			if(g_index == 8)
				g_index = 0;
		}
	}
	gpiod_set_value(motor_a,0);
	gpiod_set_value(motor_b,0);
	gpiod_set_value(motor_c,0);
	gpiod_set_value(motor_d,0);


	return 2;
}

/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_motor_drv = {
	.owner	 = THIS_MODULE,
	.write    = gpio_key_drv_write,
};


/* 从platform_device获得GPIO
 */
static int gpio_motor_probe(struct platform_device *pdev)
{	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	motor_a = gpiod_get(&pdev->dev, "a", GPIOD_OUT_LOW);
	motor_b = gpiod_get(&pdev->dev, "b", GPIOD_OUT_LOW);
	motor_c = gpiod_get(&pdev->dev, "c", GPIOD_OUT_LOW);
	motor_d = gpiod_get(&pdev->dev, "d", GPIOD_OUT_LOW);

	if (IS_ERR(motor_a) | IS_ERR(motor_b) | IS_ERR(motor_c) | IS_ERR(motor_d)) {
		dev_err(&pdev->dev, "Failed to get GPIO for motor\n");
		return PTR_ERR(motor_a);
	}
	

	/* 注册file_operations 	*/
	major = register_chrdev(0, "liqing_gpio_motor", &gpio_motor_drv);  /* /dev/gpio_key */
	gpio_motor_class = class_create(THIS_MODULE, "liqing_gpio_motor_class");
	if (IS_ERR(gpio_motor_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "liqing_gpio_motor");
		return PTR_ERR(gpio_motor_class);
	}

	device_create(gpio_motor_class, NULL, MKDEV(major, 0), NULL, "liqing_gpio_motor"); /* /dev/100ask_gpio_key */
        
    return 0;
    
}

static int gpio_motor_remove(struct platform_device *pdev)
{

	device_destroy(gpio_motor_class, MKDEV(major, 0));
	class_destroy(gpio_motor_class);
	unregister_chrdev(major, "liqing_gpio_motor");
	gpiod_put(motor_a);
	gpiod_put(motor_b);
	gpiod_put(motor_c);
	gpiod_put(motor_d);

    return 0;
}


static const struct of_device_id lqs_keys[] = {
    { .compatible = "liqing,motor" },
    { },
};

/* 1. 定义platform_driver */
static struct platform_driver gpio_motor_driver = {
    .probe      = gpio_motor_probe,
    .remove     = gpio_motor_remove,
    .driver     = {
        .name   = "liqing_gpio_motor",
        .of_match_table = lqs_keys,
    },
};

/* 2. 在入口函数注册platform_driver */
static int __init gpio_motor_init(void)
{
    int err;
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    err = platform_driver_register(&gpio_motor_driver); 
	
	return err;
}

/* 3. 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 *     卸载platform_driver
 */
static void __exit gpio_motor_exit(void)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

    platform_driver_unregister(&gpio_motor_driver);
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(gpio_motor_init);
module_exit(gpio_motor_exit);

MODULE_LICENSE("GPL");


