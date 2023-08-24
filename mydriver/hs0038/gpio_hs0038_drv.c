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
static struct class *gpio_hs0038_class;
static struct gpio_desc *hs0038_gpio;
static int irq;
static wait_queue_head_t hs0038_wq;
static u64 hs0038_edge_time[82];
static int hs0038_edge_cnt = 0;
static unsigned int hs0038_data = 1;

/* 环形缓冲区 */
#define BUF_LEN 128
static int g_keys[BUF_LEN];
static int r=0, w=0;

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



/* 0 :success
* -1 : no finish
* -2 :error
*/
static int hs0038_parse_data(unsigned int* val)
{
	u64 temp;
	unsigned char data[4];
	int i,j,m;

	if(hs0038_edge_cnt == 4)
	{
		temp = hs0038_edge_time[1] - hs0038_edge_time[0];
		if(temp > 8000000 && temp < 10000000)
		{
			temp = hs0038_edge_time[2] - hs0038_edge_time[1];
			if(temp < 3000000)
			{
				*val = hs0038_data;
				return 0;
			}
		}
	}

	m = 3;
	if(hs0038_edge_cnt >= 68)
	{
		for(i = 0;i <4; i++)
		{
			data[i] = 0;
			for(j = 0;i <8; j++)
			{
				if(hs0038_edge_time[m+1]-hs0038_edge_time[m] >1000000)
					data[i] |= (1<<j);
				m +=2;
			}
		}

		data[1] = ~data[1];
		if(data[0] != data[1])
			return -2;

		data[3] = ~data[3];
		if(data[2] != data[3])
			return -2;

		hs0038_data = (data[0] << 8) | (data[2]);
		printk(" data[0] =  %d     data[2] = %d\n",data[0],data[2]);
		*val = hs0038_data;
		return 0;
	}
	else
	{
		return -1;
	}

}

static irqreturn_t gpio_key_isr(int irq, void *dev_id)
{

	hs0038_edge_time[hs0038_edge_cnt++] = ktime_get_boottime_ns();

	if(hs0038_edge_cnt>=2)
		if(hs0038_edge_time[hs0038_edge_cnt-1]-hs0038_edge_time[hs0038_edge_cnt-2] >30000000)
		{
			hs0038_edge_time[0] = hs0038_edge_time[hs0038_edge_cnt-1];
			hs0038_edge_cnt = 1;
			return IRQ_HANDLED;
		}

	if(hs0038_edge_cnt >= 68)
	{
		printk(" edge_cnt = %d\n",hs0038_edge_cnt);
		put_key(1);
			wake_up(&hs0038_wq);
	}

	return IRQ_HANDLED;
}

/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t gpio_key_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	unsigned int val;
	unsigned char data[4];
	int i,j,m;

	if(size != 4)
		return -EAGAIN;
	wait_event_interruptible(hs0038_wq, !is_key_buf_empty());
	get_key();

		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	m = 3;
		for(i = 0;i <4; i++)
		{
			data[i] = 0;
			for(j = 0;i <8; j++)
			{
				if(hs0038_edge_time[m+1]-hs0038_edge_time[m] >1000000)
					data[i] |= (1<<j);
				m +=2;
			}
		}

		data[1] = ~data[1];
		if(data[0] != data[1])
		{
				printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

			hs0038_edge_cnt = 0;
			return -EAGAIN;
		}
		data[3] = ~data[3];
		if(data[2] != data[3])
		{
				printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
			hs0038_edge_cnt = 0;
			return -EAGAIN;
		}

		val = (data[0] << 8) | (data[2]);
		printk(" data[0] =  %d     data[2] = %d\n",data[0],data[2]);

		// printk("err = %d  edge_cnt = %d\n",err,hs0038_edge_cnt);
			hs0038_edge_cnt = 0;
			printk("get code = 0x%x\n",val);
			copy_to_user(buf, &val, 4);
			return 4;

}

/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_hs0038_drv = {
	.owner	 = THIS_MODULE,
	.read    = gpio_key_drv_read,
};


/* 从platform_device获得GPIO
 */
static int gpio_hs0038_probe(struct platform_device *pdev)
{	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	hs0038_gpio = gpiod_get(&pdev->dev, NULL, GPIOD_IN);

	if (IS_ERR(hs0038_gpio)) {
		dev_err(&pdev->dev, "Failed to get GPIO for hs0038\n");
		return PTR_ERR(hs0038_gpio);
	}
	
	init_waitqueue_head(&hs0038_wq);
	irq=gpiod_to_irq(hs0038_gpio);
	request_irq(irq, gpio_key_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "myhs0038", NULL);

	/* 注册file_operations 	*/
	major = register_chrdev(0, "liqing_gpio_hs0038", &gpio_hs0038_drv);  /* /dev/gpio_key */
	gpio_hs0038_class = class_create(THIS_MODULE, "liqing_gpio_hs0038_class");
	if (IS_ERR(gpio_hs0038_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "liqing_gpio_hs0038");
		return PTR_ERR(gpio_hs0038_class);
	}

	device_create(gpio_hs0038_class, NULL, MKDEV(major, 0), NULL, "liqing_gpio_hs0038"); /* /dev/100ask_gpio_key */
        
    return 0;
    
}

static int gpio_hs0038_remove(struct platform_device *pdev)
{

	device_destroy(gpio_hs0038_class, MKDEV(major, 0));
	class_destroy(gpio_hs0038_class);
	unregister_chrdev(major, "liqing_gpio_hs0038");
	free_irq(irq,NULL);
	gpiod_put(hs0038_gpio);

    return 0;
}


static const struct of_device_id lqs_keys[] = {
    { .compatible = "liqing,hs0038" },
    { },
};

/* 1. 定义platform_driver */
static struct platform_driver gpio_hs0038_driver = {
    .probe      = gpio_hs0038_probe,
    .remove     = gpio_hs0038_remove,
    .driver     = {
        .name   = "liqing_gpio_hs0038",
        .of_match_table = lqs_keys,
    },
};

/* 2. 在入口函数注册platform_driver */
static int __init gpio_hs0038_init(void)
{
    int err;
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    err = platform_driver_register(&gpio_hs0038_driver); 
	
	return err;
}

/* 3. 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 *     卸载platform_driver
 */
static void __exit gpio_hs0038_exit(void)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

    platform_driver_unregister(&gpio_hs0038_driver);
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(gpio_hs0038_init);
module_exit(gpio_hs0038_exit);

MODULE_LICENSE("GPL");


