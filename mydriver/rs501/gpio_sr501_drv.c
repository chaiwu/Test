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

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_sr501_class;
static struct gpio_desc *sr501_gpio;
static int irq;
static wait_queue_head_t sr501_wq;

struct fasync_struct *sr501_fasync;
struct timer_list sr501_timer;


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



static unsigned int gpio_key_drv_poll(struct file *fp, poll_table * wait)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	poll_wait(fp, &sr501_wq, wait);
	return is_key_buf_empty() ? 0 : POLLIN | POLLRDNORM;
}

static int gpio_key_drv_fasync(int fd, struct file *file, int on)
{
	if (fasync_helper(fd, file, on, &sr501_fasync) >= 0)
		return 0;
	else
		return -EIO;
}

static void sr501_timer_expire(struct timer_list *t)
{
	int vall;
	printk("gpio_sr501 timer happened\n");
	vall = gpiod_get_value(sr501_gpio);
	put_key(vall);
	wake_up(&sr501_wq);
	kill_fasync(&sr501_fasync, SIGIO, POLL_IN);
}

static irqreturn_t gpio_key_isr(int irq, void *dev_id)
{
	// int vall;
	printk("gpio_sr501 irq happened\n");
	// vall = gpiod_get_value(sr501_gpio);
	// put_key(vall);
	// wake_up(&sr501_wq);
	// kill_fasync(&sr501_fasync, SIGIO, POLL_IN);

	mod_timer(&sr501_timer, jiffies + HZ/50);

	return IRQ_WAKE_THREAD;
}

/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t gpio_key_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
#if 0
    /* Non-blocking */
	char val;
	val = gpiod_get_value(sr501_gpio);
	printk("get value  %c\n", val);
	copy_to_user(buf, &val, 1);
#endif
	/* interrupt or  poll */
	int key;
	if (is_key_buf_empty() && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;
	
	wait_event_interruptible(sr501_wq, !is_key_buf_empty());
	key = get_key();
	printk("get value  %d\n", key);
	copy_to_user(buf, &key, 4);

	return 4;
}

static ssize_t gpio_key_drv_write (struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
	char val;
	copy_from_user(&val, buf, 1);

	gpiod_set_value(sr501_gpio, val);
	return 4;
}

/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_sr501_drv = {
	.owner	 = THIS_MODULE,
	.read    = gpio_key_drv_read,
	.write    = gpio_key_drv_write,
	.poll    = gpio_key_drv_poll,
	.fasync  = gpio_key_drv_fasync,

};

/* 从platform_device获得GPIO
 */
static int gpio_sr501_probe(struct platform_device *pdev)
{	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	sr501_gpio = gpiod_get(&pdev->dev, NULL, 0);
	if (IS_ERR(sr501_gpio)) {
		dev_err(&pdev->dev, "Failed to get GPIO for sr501\n");
		return PTR_ERR(sr501_gpio);
	}

	gpiod_direction_input(sr501_gpio);
	timer_setup(&sr501_timer,sr501_timer_expire,0);
	sr501_timer.expires = ~0;
	add_timer(&sr501_timer);

	irq=gpiod_to_irq(sr501_gpio);
	request_irq(irq, gpio_key_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "sr501", NULL);

	/* 注册file_operations 	*/
	major = register_chrdev(0, "liqing_gpio_sr501", &gpio_sr501_drv);  /* /dev/gpio_key */
	gpio_sr501_class = class_create(THIS_MODULE, "liqing_gpio_sr501_class");
	if (IS_ERR(gpio_sr501_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "liqing_gpio_sr501");
		return PTR_ERR(gpio_sr501_class);
	}

	device_create(gpio_sr501_class, NULL, MKDEV(major, 0), NULL, "liqing_gpio_sr501"); /* /dev/100ask_gpio_key */
        
    return 0;
    
}

static int gpio_sr501_remove(struct platform_device *pdev)
{

	device_destroy(gpio_sr501_class, MKDEV(major, 0));
	class_destroy(gpio_sr501_class);
	unregister_chrdev(major, "liqing_gpio_sr501");
	free_irq(irq,NULL);
	del_timer(&sr501_timer);
	gpiod_put(sr501_gpio);

    return 0;
}


static const struct of_device_id lqs_keys[] = {
    { .compatible = "liqing,sr501" },
    { },
};

/* 1. 定义platform_driver */
static struct platform_driver gpio_sr501_driver = {
    .probe      = gpio_sr501_probe,
    .remove     = gpio_sr501_remove,
    .driver     = {
        .name   = "liqing_gpio_sr501",
        .of_match_table = lqs_keys,
    },
};

/* 2. 在入口函数注册platform_driver */
static int __init gpio_sr501_init(void)
{
    int err;
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	init_waitqueue_head(&sr501_wq);
    err = platform_driver_register(&gpio_sr501_driver); 
	
	return err;
}

/* 3. 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 *     卸载platform_driver
 */
static void __exit gpio_sr501_exit(void)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

    platform_driver_unregister(&gpio_sr501_driver);
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(gpio_sr501_init);
module_exit(gpio_sr501_exit);

MODULE_LICENSE("GPL");


