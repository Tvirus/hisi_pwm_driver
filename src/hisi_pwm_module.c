#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>

#include "hisi_pwm_api.h"


#define VERSION  "1.0"


#if 1
#define DEBUG(fmt, arg...)  printk("--HIPWM-- " fmt "\n", ##arg)
#else
#define DEBUG(fmt, arg...)
#endif

#define ERROR(fmt, arg...)  printk(KERN_ERR "--HIPWM-- " fmt "\n", ##arg)








static dev_t pwm_ndev;
static struct file_operations pwm_fops;
static struct cdev pwm_cdev;
static struct class *pwm_class = NULL;
static struct device *pwm_device = NULL;



static u32 pwm0_enable = 0;
static u32 pwm1_enable = 0;
static u32 pwm0_freq = 30000; /* 频率，单位Hz */
static u32 pwm1_freq = 30000;
static u32 pwm0_duty = 50;    /* 占空比，单位1% */
static u32 pwm1_duty = 50;





static ssize_t pwm0_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%u\n", pwm0_enable);
}
static ssize_t pwm0_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long en;

    if (kstrtoul(buf, 0, &en))
        return -EINVAL;

    en = !!en;
    hipwm_enable(0, (u32)en);
    pwm0_enable = en;
    return size;
}
static ssize_t pwm1_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%u\n", pwm1_enable);
}
static ssize_t pwm1_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long en;

    if (kstrtoul(buf, 0, &en))
        return -EINVAL;

    en = !!en;
    hipwm_enable(1, (u32)en);
    pwm1_enable = en;
    return size;
}
static ssize_t pwm0_freq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%u\n", pwm0_freq);
}
static ssize_t pwm0_freq_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long f;

    if (kstrtoul(buf, 0, &f))
        return -EINVAL;

    pwm0_freq = (u32)f;
    if (hipwm_set_time(0, pwm0_freq, pwm0_duty))
        return -EINVAL;
    return size;
}
static ssize_t pwm1_freq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%u\n", pwm1_freq);
}
static ssize_t pwm1_freq_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long f;

    if (kstrtoul(buf, 0, &f))
        return -EINVAL;

    pwm1_freq = (u32)f;
    if (hipwm_set_time(1, pwm1_freq, pwm1_duty))
        return -EINVAL;
    return size;
}
static ssize_t pwm0_duty_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%u\n", pwm0_duty);
}
static ssize_t pwm0_duty_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long d;

    if (kstrtoul(buf, 0, &d))
        return -EINVAL;
    if (100 < d)
        return -EINVAL;

    pwm0_duty = (u32)d;
    if (hipwm_set_time(0, pwm0_freq, pwm0_duty))
        return -EINVAL;
    return size;
}
static ssize_t pwm1_duty_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%u\n", pwm1_duty);
}
static ssize_t pwm1_duty_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long d;

    if (kstrtoul(buf, 0, &d))
        return -EINVAL;
    if (100 < d)
        return -EINVAL;

    pwm1_duty = (u32)d;
    if (hipwm_set_time(1, pwm1_freq, pwm1_duty))
        return -EINVAL;
    return size;
}


struct device_attribute pwm_attrs[] =
{
    __ATTR(pwm0_en,    0660, pwm0_enable_show, pwm0_enable_store),
    __ATTR(pwm1_en,    0660, pwm1_enable_show, pwm1_enable_store),
    __ATTR(pwm0_freq,  0660, pwm0_freq_show,   pwm0_freq_store),
    __ATTR(pwm1_freq,  0660, pwm1_freq_show,   pwm1_freq_store),
    __ATTR(pwm0_duty,  0660, pwm0_duty_show,   pwm0_duty_store),
    __ATTR(pwm1_duty,  0660, pwm1_duty_show,   pwm1_duty_store)
};
int pwm_attrs_size = sizeof(pwm_attrs)/sizeof(pwm_attrs[0]);

static int pwm_add_sysfs_interfaces(struct device *dev, struct device_attribute *a, int size)
{
    int i;

    for (i = 0; i < size; i++)
        if (device_create_file(dev, a + i))
            goto undo;
    return 0;

undo:
    for (i--; i >= 0; i--)
        device_remove_file(dev, a + i);
    return -ENODEV;
}
static void pwm_del_sysfs_interfaces(struct device *dev, struct device_attribute *a, int size)
{
    int i;

    for (i = 0; i < size; i++)
        device_remove_file(dev, a + i);
}





static int pwm_open(struct inode* inode, struct file* filp)
{
    return -EPERM;
}

static int pwm_release(struct inode* inode, struct file* filp)
{
    return -EPERM;
}




static int __init pwm_init(void)
{
    int ret = 0;

    DEBUG("Driver Version: %s", VERSION);

    ret = alloc_chrdev_region(&pwm_ndev, 0, 1, "hisi_pwm");
    if (ret)
    {
        ERROR("alloc chrdev region failed !");
        goto ERR_1;
    }

    pwm_fops.owner   = THIS_MODULE,
    pwm_fops.open    = pwm_open,
    pwm_fops.release = pwm_release,
    cdev_init(&pwm_cdev, &pwm_fops);
    ret = cdev_add(&pwm_cdev, pwm_ndev, 1);
    if (ret)
    {
        ERROR("failed to add char dev !");
        goto ERR_2;
    }

    pwm_class = class_create(THIS_MODULE, "hisi_pwm");
    if (IS_ERR_OR_NULL(pwm_class))
    {
        ERROR("failed to create class \"hisi_pwm\" !");
        ret = PTR_ERR(pwm_class);
        goto ERR_3;
    }
    pwm_device = device_create(pwm_class, NULL, pwm_ndev, NULL, "hipwm");
    if (IS_ERR_OR_NULL(pwm_device))
    {
        ERROR("failed to create class device !");
        ret = PTR_ERR(pwm_device);
        goto ERR_4;
    }

    hipwm_init();
    hipwm_set_time(0, pwm0_freq, pwm0_duty);
    hipwm_set_time(1, pwm1_freq, pwm1_duty);

    ret = pwm_add_sysfs_interfaces(pwm_device, pwm_attrs, pwm_attrs_size);
    if (ret)
    {
        ERROR("create sysfs interface failed !");
        goto ERR_5;
    }

    return 0;




ERR_5:
    hipwm_deinit();
    device_destroy(pwm_class, pwm_ndev);
ERR_4:
    class_destroy(pwm_class);
ERR_3:
    cdev_del(&pwm_cdev);
ERR_2:
    unregister_chrdev_region(pwm_ndev, 1);
ERR_1:
    return ret;
}
static void __exit pwm_exit(void)
{
    DEBUG("exit");

    hipwm_enable(0, 0);
    hipwm_enable(1, 0);
    pwm_del_sysfs_interfaces(pwm_device, pwm_attrs, pwm_attrs_size);
    hipwm_deinit();
    device_destroy(pwm_class, pwm_ndev);
    class_destroy(pwm_class);
    cdev_del(&pwm_cdev);
    unregister_chrdev_region(pwm_ndev, 1);
}


late_initcall(pwm_init);
module_exit(pwm_exit);


MODULE_AUTHOR("LLL");
MODULE_DESCRIPTION("hisi pwm driver");
MODULE_LICENSE("GPL");
