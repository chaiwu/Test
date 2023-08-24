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
static struct class *gpio_ds18b20_class;
static struct gpio_desc *ds18b20_gpio;
static wait_queue_head_t ds18b20_wq;


void ds18b20_delay_us(int us)
{
	u64 pre,last;
	pre = ktime_get_boottime_ns();
	while(1)
	{
		last = ktime_get_boottime_ns();
		if(last - pre >= us *1000)
			break;
	}
}


static int ds18b20_wait_for_ack(void)
{
	int timeout_count = 500;
	/* wait for low */
	while(gpiod_get_value(ds18b20_gpio) && --timeout_count)
	{
		udelay(1);
	}

	if(!timeout_count)
		return -1;

	/* now is low  ,wait for high*/
	timeout_count = 500;
	while(!gpiod_get_value(ds18b20_gpio) && --timeout_count)
	{
		udelay(1);
	}
	if(!timeout_count)
		return -1;

	return 0;
}

static int ds18b20_reset(void)
{
	gpiod_direction_output(ds18b20_gpio,0);
	ds18b20_delay_us(480);
	gpiod_direction_input(ds18b20_gpio);

	if(ds18b20_wait_for_ack())
		return -1;
	else
		return 0;
}

static unsigned char ds18b20_read_byte(void)
{
	int i;
	unsigned char data = 0;

	for (i = 0; i < 8; i++)
	{
		// data <<= 1;
		gpiod_direction_output(ds18b20_gpio,0);
		ds18b20_delay_us(2);
		gpiod_direction_input(ds18b20_gpio);
		ds18b20_delay_us(7);

		if (gpiod_get_value(ds18b20_gpio) == 1)
			data |= (1<<i);

		ds18b20_delay_us(60);
		
	}

	return data;
}

static void ds18b20_write_byte(unsigned char data)
{
	/*
	优先传输最低位
	*/
	int i;
	for (i = 0; i < 8; i++)
	{ 
		if (data & (1<<i))
		{
			/* out 1 */
			gpiod_direction_output(ds18b20_gpio,0);
			ds18b20_delay_us(2);

			gpiod_direction_input(ds18b20_gpio);
			ds18b20_delay_us(60);
		}
		else
		{
			/* out 0 */
			gpiod_direction_output(ds18b20_gpio,0);
			ds18b20_delay_us(60);
	
			gpiod_direction_input(ds18b20_gpio);
			ds18b20_delay_us(2);

		}
	}
}

/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t gpio_key_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	unsigned char tempL=0,tempH=0;
	unsigned int integer;
	unsigned char decimal1,decimal2,decimal;
	unsigned long flags;

	if(size!=5)
		return -EINVAL;

	local_irq_save(flags);       //close irq

	if(ds18b20_reset())
	{
		gpiod_direction_output(ds18b20_gpio,1);
		local_irq_restore(flags);
		return -ENODEV;
	}

	ds18b20_write_byte(0xcc);    //ignore rom
	ds18b20_write_byte(0x44);    //temp transformation

	gpiod_direction_output(ds18b20_gpio,1);

	local_irq_restore(flags);    //open irq
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(HZ);         //delay 1s

	local_irq_save(flags);       //close irq
	if(ds18b20_reset())
	{
		gpiod_direction_output(ds18b20_gpio,1);
		local_irq_restore(flags);
		return -ENODEV;
	}

	ds18b20_write_byte(0xcc);    //ignore rom
	ds18b20_write_byte(0xbe);    //start read 
	tempL = ds18b20_read_byte();					//读温度低8位
	tempH = ds18b20_read_byte();					//读温度高8位
	if(tempH>0x7f)      							//最高位为1时温度是负
	{
		tempL    = ~tempL;         						//补码转换，取反加一
		tempH    = ~tempH+1;      
		integer  = tempL/16+tempH*16;      			//整数部分
		decimal1 = (tempL&0x0f)*10/16; 				//小数第一位
		decimal2 = (tempL&0x0f)*100/16%10;			//小数第二位
		decimal  = decimal1*10+decimal2; 			//小数两位
    }
	else
	{
		integer  = tempL/16+tempH*16;      				//整数部分
		decimal1 = (tempL&0x0f)*10/16; 					//小数第一位
		decimal2 = (tempL&0x0f)*100/16%10;				//小数第二位
		decimal  = decimal1*10+decimal2; 				//小数两位
	}


	local_irq_restore(flags);    //open irq

	gpiod_direction_output(ds18b20_gpio,1);
	copy_to_user(buf,&integer,4);
	copy_to_user(buf+4,&decimal,1);

	return 5;

}

/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_ds18b20_drv = {
	.owner	 = THIS_MODULE,
	.read    = gpio_key_drv_read,
};


/* 从platform_device获得GPIO
 */
static int gpio_ds18b20_probe(struct platform_device *pdev)
{	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	ds18b20_gpio = gpiod_get(&pdev->dev, NULL, GPIOD_OUT_HIGH);

	if (IS_ERR(ds18b20_gpio)) {
		dev_err(&pdev->dev, "Failed to get GPIO for ds18b20\n");
		return PTR_ERR(ds18b20_gpio);
	}
	
	init_waitqueue_head(&ds18b20_wq);

	/* 注册file_operations 	*/
	major = register_chrdev(0, "liqing_gpio_ds18b20", &gpio_ds18b20_drv);  /* /dev/gpio_key */
	gpio_ds18b20_class = class_create(THIS_MODULE, "liqing_gpio_ds18b20_class");
	if (IS_ERR(gpio_ds18b20_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "liqing_gpio_ds18b20");
		return PTR_ERR(gpio_ds18b20_class);
	}

	device_create(gpio_ds18b20_class, NULL, MKDEV(major, 0), NULL, "liqing_gpio_ds18b20"); /* /dev/100ask_gpio_key */
        
    return 0;
    
}

static int gpio_ds18b20_remove(struct platform_device *pdev)
{

	device_destroy(gpio_ds18b20_class, MKDEV(major, 0));
	class_destroy(gpio_ds18b20_class);
	unregister_chrdev(major, "liqing_gpio_ds18b20");
	gpiod_put(ds18b20_gpio);

    return 0;
}


static const struct of_device_id lqs_keys[] = {
    { .compatible = "liqing,ds18b20" },
    { },
};

/* 1. 定义platform_driver */
static struct platform_driver gpio_ds18b20_driver = {
    .probe      = gpio_ds18b20_probe,
    .remove     = gpio_ds18b20_remove,
    .driver     = {
        .name   = "liqing_gpio_ds18b20",
        .of_match_table = lqs_keys,
    },
};

/* 2. 在入口函数注册platform_driver */
static int __init gpio_ds18b20_init(void)
{
    int err;
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    err = platform_driver_register(&gpio_ds18b20_driver); 
	
	return err;
}

/* 3. 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 *     卸载platform_driver
 */
static void __exit gpio_ds18b20_exit(void)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

    platform_driver_unregister(&gpio_ds18b20_driver);
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(gpio_ds18b20_init);
module_exit(gpio_ds18b20_exit);

MODULE_LICENSE("GPL");


