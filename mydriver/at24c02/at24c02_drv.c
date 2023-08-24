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
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <asm/current.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>

#define IOC_AT24C02_READ 100
#define IOC_AT24C02_WRITE 101

/* 主设备号                                                                 */
static int major = 0;
static struct class *at24c02_class;
static struct i2c_client *at24c02_client;


static long at24c02_drv_ioctl(struct file * file,unsigned int cmd,unsigned long arg)
{
	struct i2c_msg msgs[2];
	unsigned char addr;
	unsigned char data;
	unsigned char ker_buf[2];
	// unsigned char byte_buf[2];

	copy_from_user(ker_buf,(const void __user *)arg, 2);
	addr = ker_buf[0];

	printk("kerbuf = %c, %c\n",ker_buf[0],ker_buf[1]);
	switch (cmd)
	{
		case IOC_AT24C02_READ:
		{
			msgs[0].addr = at24c02_client->addr;
			msgs[0].flags = 0;  /* write */
			msgs[0].len = 1;   /* 1 byte */  
			msgs[0].buf = &addr;

			msgs[1].addr = at24c02_client->addr;
			msgs[1].flags = I2C_M_RD;  /* read */
			msgs[1].len = 1;   /* 1 byte */  
			msgs[1].buf = &data;

			i2c_transfer(at24c02_client->adapter,msgs,2);

			ker_buf[1] = data;
			copy_to_user(( void __user *)arg,ker_buf,2);

			break;
		}
		case IOC_AT24C02_WRITE:
		{
			msgs[0].addr = at24c02_client->addr;
			msgs[0].flags = 0;  /* write */
			msgs[0].len = 2;   /* 2 byte */  
			msgs[0].buf = ker_buf;

			i2c_transfer(at24c02_client->adapter,msgs,1);
			mdelay(20);

			break;
		}
	}

	return 0;

}

/* 定义自己的file_operations结构体                                              */
static struct file_operations at24c02_drv = {
	.owner	 = THIS_MODULE,
	.unlocked_ioctl    = at24c02_drv_ioctl,
};


/* 从platform_device获得GPIO
 */
static int at24c02_probe(struct i2c_client *client,const struct i2c_device_id *id)
{	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	at24c02_client = client;

	/* 注册file_operations 	*/
	major = register_chrdev(0, "liqing_at24c02", &at24c02_drv);  /* /dev/gpio_key */
	at24c02_class = class_create(THIS_MODULE, "liqing_at24c02_class");
	if (IS_ERR(at24c02_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "liqing_at24c02");
		return PTR_ERR(at24c02_class);
	}

	device_create(at24c02_class, NULL, MKDEV(major, 0), NULL, "liqing_at24c02"); /* /dev/100ask_gpio_key */
        
    return 0;
    
}

static int at24c02_remove(struct i2c_client *client)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	device_destroy(at24c02_class, MKDEV(major, 0));
	class_destroy(at24c02_class);
	unregister_chrdev(major, "liqing_at24c02");

    return 0;
}


static const struct of_device_id lqs_keys[] = {
    { .compatible = "liqing,at24c02" },
    { },
};

static const struct i2c_device_id at24c02_ids[] = {
    { "liqing", (kernel_ulong_t)NULL },
    { }
};

/* 1. 定义platform_driver */
static struct i2c_driver at24c02_driver = {
    .probe      = at24c02_probe,
    .remove     = at24c02_remove,
    .driver     = {
        .name   = "liqing_at24c02",
        .of_match_table = lqs_keys,
    },
	.id_table   = at24c02_ids,
};

/* 2. 在入口函数注册platform_driver */
static int __init at24c02_init(void)
{
    int err;
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    err = i2c_add_driver(&at24c02_driver); 
	
	return err;
}

/* 3. 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 *     卸载platform_driver
 */
static void __exit at24c02_exit(void)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

    i2c_del_driver(&at24c02_driver);
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(at24c02_init);
module_exit(at24c02_exit);

MODULE_LICENSE("GPL");


