﻿#include <linux/module.h>
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

#define IOC_OLED_CLEAR 100
#define IOC_OLED_WRITE 101
#define IOC_OLED_SETXY 102
#define IOC_OLED_KUNKUN 103

//为0 表示命令，为1表示数据
#define OLED_CMD 	0
#define OLED_DATA 	1

/* 主设备号                                                                 */
static int major = 0;
static struct class *oled_class;
static struct i2c_client *oled_client;

static void oled_write_cmd_data(unsigned char uc_data,unsigned char uc_cmd)
{

	struct i2c_msg msgs[1];
	char data;
	if(uc_cmd==0)
	{
		// *GPIO4_DR_s &= ~(1<<20);//拉低，表示写入指令
		data = 0x00;
		msgs[0].addr = oled_client->addr;
		msgs[0].flags = 0;  /* write */
		msgs[0].len = 1;   /* 1 byte */  
		msgs[0].buf = &data;
		i2c_transfer(oled_client->adapter,msgs,1);

	}
	else
	{
		// *GPIO4_DR_s |= (1<<20);//拉高，表示写入数据
		data = 0x40;
		msgs[0].addr = oled_client->addr;
		msgs[0].flags = 0;  /* write */
		msgs[0].len = 1;   /* 1 byte */  
		msgs[0].buf = &data;
		i2c_transfer(oled_client->adapter,msgs,1);
	}
	// mdelay(10);
	//spi_writeread(ESCPI1_BASE,uc_data);//写入
	msgs[0].addr = oled_client->addr;
	msgs[0].flags = 0;  /* write */
	msgs[0].len = 1;   /* 1 byte */  
	msgs[0].buf = &uc_data;
	i2c_transfer(oled_client->adapter,msgs,1);
	// mdelay(20);

}

static int oled_hardware_init(void)
{		  
	
	mdelay(200);	  						  					  				 	   		  	  	 	  
	oled_write_cmd_data(0xae,OLED_CMD);//关闭显示

	oled_write_cmd_data(0x00,OLED_CMD);//设置 lower column address
	oled_write_cmd_data(0x10,OLED_CMD);//设置 higher column address

	oled_write_cmd_data(0x40,OLED_CMD);//设置 display start line

	oled_write_cmd_data(0xB0,OLED_CMD);//设置page address

	oled_write_cmd_data(0x81,OLED_CMD);// contract control
	oled_write_cmd_data(0xff,OLED_CMD);//128

	oled_write_cmd_data(0xa1,OLED_CMD);//设置 segment remap

	oled_write_cmd_data(0xa6,OLED_CMD);//normal /reverse

	oled_write_cmd_data(0xa8,OLED_CMD);//multiple ratio
	oled_write_cmd_data(0x3f,OLED_CMD);//duty = 1/64

	oled_write_cmd_data(0xc8,OLED_CMD);//com scan direction

	oled_write_cmd_data(0xd3,OLED_CMD);//set displat offset
	oled_write_cmd_data(0x00,OLED_CMD);//

	oled_write_cmd_data(0xd5,OLED_CMD);//set osc division
	oled_write_cmd_data(0x80,OLED_CMD);//

	oled_write_cmd_data(0xD8,OLED_CMD);//set area color mode off
	oled_write_cmd_data(0x05,OLED_CMD);//

	oled_write_cmd_data(0xd9,OLED_CMD);//ser pre-charge period
	oled_write_cmd_data(0x1f,OLED_CMD);//

	oled_write_cmd_data(0xda,OLED_CMD);//set com pins
	oled_write_cmd_data(0x12,OLED_CMD);//

	oled_write_cmd_data(0xdb,OLED_CMD);//set vcomh
	oled_write_cmd_data(0x30,OLED_CMD);//

	oled_write_cmd_data(0x8d,OLED_CMD);//set charge pump disable 
	oled_write_cmd_data(0x14,OLED_CMD);//

	oled_write_cmd_data(0xaf,OLED_CMD);//set dispkay on

	return 0;
}	

static void OLED_DIsp_Set_Pos(int x, int y)
{ 	
	oled_write_cmd_data(0xb0+y,OLED_CMD);
	oled_write_cmd_data((x&0x0f),OLED_CMD); 
	oled_write_cmd_data(((x&0xf0)>>4)|0x10,OLED_CMD);
}   


static void OLED_DIsp_Clear(void)  
{
    unsigned char x, y;
    for (y = 0; y < 8; y++)
    {
        OLED_DIsp_Set_Pos(0, y);
        for (x = 0; x < 128; x++)
            oled_write_cmd_data(0xff, OLED_DATA); /* 清零 */
    }
}

static long oled_drv_ioctl(struct file * file,unsigned int cmd,unsigned long arg)
{

	return 0;

}

/* 定义自己的file_operations结构体                                              */
static struct file_operations oled_drv = {
	.owner	 = THIS_MODULE,
	.unlocked_ioctl    = oled_drv_ioctl,
};


/* 从platform_device获得GPIO
 */
static int oled_probe(struct i2c_client *client,const struct i2c_device_id *id)
{	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	oled_client = client;

	/* 注册file_operations 	*/
	major = register_chrdev(0, "liqing_oled", &oled_drv);  /* /dev/gpio_key */
	oled_class = class_create(THIS_MODULE, "liqing_oled_class");
	if (IS_ERR(oled_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "liqing_oled");
		return PTR_ERR(oled_class);
	}

	device_create(oled_class, NULL, MKDEV(major, 0), NULL, "liqing_oled"); /* /dev/100ask_gpio_key */
    oled_hardware_init();
	OLED_DIsp_Clear();

    return 0;
    
}

static int oled_remove(struct i2c_client *client)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	device_destroy(oled_class, MKDEV(major, 0));
	class_destroy(oled_class);
	unregister_chrdev(major, "liqing_oled");

    return 0;
}


static const struct of_device_id lqs_keys[] = {
    { .compatible = "liqing,i2coled" },
    { },
};

static const struct i2c_device_id oled_ids[] = {
    { "liqing", (kernel_ulong_t)NULL },
    { }
};

/* 1. 定义platform_driver */
static struct i2c_driver oled_driver = {
    .probe      = oled_probe,
    .remove     = oled_remove,
    .driver     = {
        .name   = "liqing_oled",
        .of_match_table = lqs_keys,
    },
	.id_table   = oled_ids,
};

/* 2. 在入口函数注册platform_driver */
static int __init oled_init(void)
{
    int err;
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    err = i2c_add_driver(&oled_driver); 
	
	return err;
}

/* 3. 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 *     卸载platform_driver
 */
static void __exit oled_exit(void)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

    i2c_del_driver(&oled_driver);
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(oled_init);
module_exit(oled_exit);

MODULE_LICENSE("GPL");


