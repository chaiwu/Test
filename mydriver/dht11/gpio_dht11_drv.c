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
static struct class *gpio_dht11_class;
static struct gpio_desc *dht11_gpio;
static int irq;
static wait_queue_head_t dht11_wq;
static u64 dht11_edge_time[82];
static int dht11_edge_cnt = 0;
static int dht11_data = 0;



static void dht11_reset(void)
{
	gpiod_direction_output(dht11_gpio,1);
}

static void dht11_start(void)
{
	mdelay(30);
	gpiod_set_value(dht11_gpio,0);
	mdelay(20);
	gpiod_set_value(dht11_gpio,1);
	udelay(40);
	gpiod_direction_input(dht11_gpio);
	udelay(2);
}

static int dht11_wait_for_ready(void)
{
	int timeout_us = 200000;

	/* wait for low  */
	while(gpiod_get_value(dht11_gpio) && --timeout_us)
	{
		udelay(1);
	}
	if(!timeout_us)
	{
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		return -1;
	}

	/* now is low and wait for high */
	timeout_us = 200;
	while(!gpiod_get_value(dht11_gpio) && --timeout_us)
	{
		udelay(1);
	}

	if(!timeout_us)
	{
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		return -1;
	}

	/* now is high and wait for low */
	timeout_us = 200;
	while(gpiod_get_value(dht11_gpio) && --timeout_us)
	{
		udelay(1);
	}

	if(!timeout_us)
	{
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		return -1;
	}
	return 0;

}

static int dht11_parse_data(char* data)
{
	int i,j,m = 0;
	for(i =0;i<5;i++)
	{
		data[i] = 0;
		for(j=0;j<8;j++)
		{
			data[i] <<= 1;
			if(dht11_edge_time[m+1]-dht11_edge_time[m]>=40000)
				data[i] |=1;
			m +=2;
		}
	}
	if(data[4]!= data[0]+data[1]+data[2]+data[3])
		return -1;
	else
		return 0;

}

static irqreturn_t gpio_key_isr(int irq, void *dev_id)
{
	dht11_edge_time[dht11_edge_cnt++] = ktime_get_boottime_ns();
	if(dht11_edge_cnt>=80)
	{
		dht11_data = 1;
		wake_up(&dht11_wq);
	}

	return IRQ_HANDLED;
}

/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t gpio_key_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	unsigned char data[5];
	int timeout;

	if(size!=4)
		return -EINVAL;

	dht11_reset();
	dht11_start();

	if(dht11_wait_for_ready())
	{
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		dht11_reset();
		return -EAGAIN;
	}

	dht11_edge_cnt = 0;
	dht11_data = 0;

	request_irq(irq, gpio_key_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "mydht11", NULL);
	timeout = wait_event_timeout(dht11_wq, dht11_data,HZ);
	free_irq(irq,NULL);
	dht11_reset();

	if(!timeout)
	{
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		return -ETIMEDOUT;
	}

	if(!dht11_parse_data(data))
	{
		copy_to_user(buf, data, 4);
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		return 4;
	}
	else
	{
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		return -EAGAIN;
	}

}

/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_dht11_drv = {
	.owner	 = THIS_MODULE,
	.read    = gpio_key_drv_read,
};


/* 从platform_device获得GPIO
 */
static int gpio_dht11_probe(struct platform_device *pdev)
{	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	dht11_gpio = gpiod_get(&pdev->dev, NULL, GPIOD_OUT_LOW);

	if (IS_ERR(dht11_gpio)) {
		dev_err(&pdev->dev, "Failed to get GPIO for dht11\n");
		return PTR_ERR(dht11_gpio);
	}
	
	init_waitqueue_head(&dht11_wq);
	irq=gpiod_to_irq(dht11_gpio);

	/* 注册file_operations 	*/
	major = register_chrdev(0, "liqing_gpio_dht11", &gpio_dht11_drv);  /* /dev/gpio_key */
	gpio_dht11_class = class_create(THIS_MODULE, "liqing_gpio_dht11_class");
	if (IS_ERR(gpio_dht11_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "liqing_gpio_dht11");
		return PTR_ERR(gpio_dht11_class);
	}

	device_create(gpio_dht11_class, NULL, MKDEV(major, 0), NULL, "liqing_gpio_dht11"); /* /dev/100ask_gpio_key */
        
    return 0;
    
}

static int gpio_dht11_remove(struct platform_device *pdev)
{

	device_destroy(gpio_dht11_class, MKDEV(major, 0));
	class_destroy(gpio_dht11_class);
	unregister_chrdev(major, "liqing_gpio_dht11");
	gpiod_put(dht11_gpio);

    return 0;
}


static const struct of_device_id lqs_keys[] = {
    { .compatible = "liqing,dht11" },
    { },
};

/* 1. 定义platform_driver */
static struct platform_driver gpio_dht11_driver = {
    .probe      = gpio_dht11_probe,
    .remove     = gpio_dht11_remove,
    .driver     = {
        .name   = "liqing_gpio_dht11",
        .of_match_table = lqs_keys,
    },
};

/* 2. 在入口函数注册platform_driver */
static int __init gpio_dht11_init(void)
{
    int err;
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    err = platform_driver_register(&gpio_dht11_driver); 
	
	return err;
}

/* 3. 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 *     卸载platform_driver
 */
static void __exit gpio_dht11_exit(void)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

    platform_driver_unregister(&gpio_dht11_driver);
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(gpio_dht11_init);
module_exit(gpio_dht11_exit);

MODULE_LICENSE("GPL");


