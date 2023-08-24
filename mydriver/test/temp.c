#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#define I2C_BUS_NUM 0   // I2C控制器编号
#define AT24C02_ADDR 0x50   // AT24C02设备地址

static struct i2c_client *at24c02_client;   // I2C设备客户端
static struct class *at24c02_class;
static int MAJOR_NUM = 0;
static struct cdev at24c02_cdev;

static int at24c02_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int at24c02_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t at24c02_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    struct i2c_msg msgs[2];
    u8 data[2];
    int ret;
    
    msgs[0].addr = at24c02_client->addr;
    msgs[0].flags = 0;
    msgs[0].buf = data;
    msgs[0].len = 1;
    
    msgs[1].addr = at24c02_client->addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].buf = data + 1;
    msgs[1].len = 1;
    
    ret = i2c_transfer(at24c02_client->adapter, msgs, 2);
    
    if (ret == 2) {
        if (copy_to_user(buf, data, 2))
            return -EFAULT;
        return 2;
    }
    
    return ret < 0 ? ret : -EIO;
}

static ssize_t at24c02_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    struct i2c_msg msg;
    u8 data[3];
    int ret;
    
    if (count != 3)
        return -EINVAL;
    
    if (copy_from_user(data, buf, 3))
        return -EFAULT;
    
    msg.addr = at24c02_client->addr;
    msg.flags = 0;
    msg.buf = data;
    msg.len = 3;
    ret = i2c_transfer(at24c02_client->adapter, &msg, 1);
    
    if (ret == 1)
        return 3;
    
    return ret < 0 ? ret : -EIO;
}

static const struct file_operations at24c02_fops = {
    .owner = THIS_MODULE,
    .open = at24c02_open,
    .release = at24c02_release,
    .read = at24c02_read,
    .write = at24c02_write,
};

static struct i2c_device_id at24c02_idtable[] = {
    { "atmel,24c02", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, at24c02_idtable);

static struct of_device_id at24c02_of_match[] = {
    { .compatible = "atmel,24c02" },
    { }
};
MODULE_DEVICE_TABLE(of, at24c02_of_match);

static int at24c02_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct device *dev;
    dev_t devno;
    int ret;
    
    at24c02_client = client;
    
    // 注册字符设备驱动
    devno = MKDEV(MAJOR_NUM, 0);
    ret = register_chrdev_region(devno, 1, "at24c02");
    if (ret < 0) {
        return ret;
    }
    
    cdev_init(&at24c02_cdev, &at24c02_fops);
    at24c02_cdev.owner = THIS_MODULE;
    ret = cdev_add(&at24c02_cdev, devno, 1);
    if (ret < 0) {
        unregister_chrdev_region(devno, 1);
        return ret;
    }
    
    // 为设备创建设备节点
    dev = device_create(at24c02_class, NULL, devno, NULL, "at24c02");
    if (IS_ERR(dev)) {
        cdev_del(&at24c02_cdev);
        unregister_chrdev_region(devno, 1);
        return PTR_ERR(dev);
    }
    
    return 0;
}

static int at24c02_remove(struct i2c_client *client)
{
    dev_t devno;
    
    devno = MKDEV(MAJOR_NUM, 0);
    device_destroy(at24c02_class, devno);
    cdev_del(&at24c02_cdev);
    unregister_chrdev_region(devno, 1);
    
    return 0;
}

static struct i2c_driver at24c02_driver = {
    .probe = at24c02_probe,
    .remove = at24c02_remove,
    .id_table = at24c02_idtable,
    .driver = {
        .name = "at24c02",
        .of_match_table = at24c02_of_match,
    },
};

static int __init at24c02_init(void)
{
    int ret;
    
    ret = i2c_add_driver(&at24c02_driver);
    if (ret < 0) {
        return ret;
    }
    
    at24c02_class = class_create(THIS_MODULE, "at24c02");
    if (IS_ERR(at24c02_class)) {
        i2c_del_driver(&at24c02_driver);
        return PTR_ERR(at24c02_class);
    }
    
    return 0;
}

static void __exit at24c02_exit(void)
{
    class_destroy(at24c02_class);
    i2c_del_driver(&at24c02_driver);
}

module_init(at24c02_init);
module_exit(at24c02_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AT24C02 EEPROM I2C Driver");
