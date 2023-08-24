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
static struct class *gpio_sr04_class;
static struct gpio_desc *sr04_trig;
static struct gpio_desc *sr04_echo;
static int irq;
static wait_queue_head_t sr04_wq;
static u64 sr04_data_ns = 0;

/* 环形缓冲区 */
#define BUF_LEN 128
static int g_keys[BUF_LEN];
static int r, w;

#define NEXT_POS(x) ((x+1) % BUF_LEN)

static int is_key_buf_empty(void)
{
	return (r == w);
}

static int is_key_buf_full(void)
{
	return (r == NEXT_POS(w));
}

static void put_key(int key)
{
	if (!is_key_buf_full())
	{
		g_keys[w] = key;
		w = NEXT_POS(w);
	}
}

static int get_key(void)
{
	int key = 0;
	if (!is_key_buf_empty())
	{
		key = g_keys[r];
		r = NEXT_POS(r);
	}
	return key;
}




/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t gpio_key_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	int timeout = 1000000;
	// int us = 0;
	// unsigned long flags;

	// local_irq_save(flags);

	gpiod_set_value(sr04_trig,1);
	udelay(15);
	gpiod_set_value(sr04_trig,0);

	// while(!gpiod_get_value(sr04_echo)&&timeout)
	// {
	// 	udelay(1);
	// 	timeout--;
	// }
	// if(!timeout)
	// {
	// 	local_irq_restore(flags);
	// 	return -EAGAIN;
	// }

	// timeout = 1000000;
	// while(gpiod_get_value(sr04_echo)&&timeout)
	// {
	// 	udelay(1);
	// 	us++;
	// 	timeout--;
	// }
	// if(!timeout)
	// {
	// 	local_irq_restore(flags);
	// 	return -EAGAIN;
	// }

	// local_irq_restore(flags);
	// copy_to_user(buf,&us,4);
	// return 4;

	timeout = wait_event_interruptible_timeout(sr04_wq, !is_key_buf_empty(),HZ);
	if(timeout)
	{
		sr04_data_ns = get_key();
		printk("get value  %lld\n", sr04_data_ns);
		copy_to_user(buf, &sr04_data_ns, 4);
		sr04_data_ns = 0;
		return 4;
	}
	else
	{
		return -EAGAIN;
	}
	// return 0;
}

/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_sr04_drv = {
	.owner	 = THIS_MODULE,
	.read    = gpio_key_drv_read,
};

static irqreturn_t gpio_key_isr(int irq, void *dev_id)
{
	int vall = gpiod_get_value(sr04_echo);

	if(vall)
	{
		sr04_data_ns = ktime_get_ns();
	}
	else{
		sr04_data_ns = ktime_get_ns() - sr04_data_ns;
		put_key(sr04_data_ns);
		wake_up(&sr04_wq);

	}

	return IRQ_HANDLED;
}

/* 从platform_device获得GPIO
 */
static int gpio_sr04_probe(struct platform_device *pdev)
{	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	sr04_trig = gpiod_get(&pdev->dev, "echo", GPIOD_OUT_LOW);
	sr04_echo = gpiod_get(&pdev->dev, "trig", GPIOD_IN);

	if (IS_ERR(sr04_trig) | IS_ERR(sr04_echo)) {
		dev_err(&pdev->dev, "Failed to get GPIO for sr04\n");
		return PTR_ERR(sr04_trig);
	}
	
	init_waitqueue_head(&sr04_wq);
	irq=gpiod_to_irq(sr04_echo);
	request_irq(irq, gpio_key_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "sr04", NULL);

	/* 注册file_operations 	*/
	major = register_chrdev(0, "liqing_gpio_sr04", &gpio_sr04_drv);  /* /dev/gpio_key */
	gpio_sr04_class = class_create(THIS_MODULE, "liqing_gpio_sr04_class");
	if (IS_ERR(gpio_sr04_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "liqing_gpio_sr04");
		return PTR_ERR(gpio_sr04_class);
	}

	device_create(gpio_sr04_class, NULL, MKDEV(major, 0), NULL, "liqing_gpio_sr04"); /* /dev/100ask_gpio_key */
        
    return 0;
    
}

static int gpio_sr04_remove(struct platform_device *pdev)
{

	device_destroy(gpio_sr04_class, MKDEV(major, 0));
	free_irq(irq,NULL);
	class_destroy(gpio_sr04_class);
	unregister_chrdev(major, "liqing_gpio_sr04");
	gpiod_put(sr04_trig);
	gpiod_put(sr04_echo);

    return 0;
}


static const struct of_device_id lqs_keys[] = {
    { .compatible = "liqing,sr04" },
    { },
};

/* 1. 定义platform_driver */
static struct platform_driver gpio_sr04_driver = {
    .probe      = gpio_sr04_probe,
    .remove     = gpio_sr04_remove,
    .driver     = {
        .name   = "liqing_gpio_sr04",
        .of_match_table = lqs_keys,
    },
};

/* 2. 在入口函数注册platform_driver */
static int __init gpio_sr04_init(void)
{
    int err;
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    err = platform_driver_register(&gpio_sr04_driver); 
	
	return err;
}

/* 3. 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 *     卸载platform_driver
 */
static void __exit gpio_sr04_exit(void)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

    platform_driver_unregister(&gpio_sr04_driver);
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(gpio_sr04_init);
module_exit(gpio_sr04_exit);

MODULE_LICENSE("GPL");


