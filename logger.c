#include<linux/init.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/fs.h>
#include<linux/cdev.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EVENT LOGGER PROJECT");
MODULE_AUTHOR("JAYARAJ");


/*------------Macros-------------------------------*/
#define BASEMINOR    0
#define COUNT        1
#define DEVICE_NAME  "eventlogger"
/***************************************************/

/*------------structure declaration----------------*/
dev_t event_dev;

static struct cdev *event_cdev;

static struct class *event_class;

static struct device *event_device;
/******************************************************/

/*-----------function prototype----------------------*/
static int event_open(struct inode *inode,struct file *file);
static int event_release(struct inode *inode,struct file *file);
/*****************************************************/

static int event_open(struct inode *inode,struct file *file){
	pr_info("File opened successfully...\n");
	return 0;
}
static int event_release(struct inode *inode,struct file *file){
	pr_info("File closed successfully...");
	return 0;
}
static struct file_operations event_fops = {
	.open = event_open,
	.release = event_release
};

static int __init eventInit(void){
	int ret = 0;
	ret = alloc_chrdev_region(&event_dev,BASEMINOR,COUNT,DEVICE_NAME);
	if(ret < 0){
		pr_err("Failed to allocate device number\n");
		return ret;
	}
	event_cdev = cdev_alloc();
	if(!event_cdev){
		unregister_chrdev_region(event_dev,COUNT);
		return -ENOMEM;
	}

	event_cdev->ops = &event_fops;
	event_cdev->owner = THIS_MODULE;

	ret = cdev_add(event_cdev,event_dev,COUNT);
	if(ret < 0){
		cdev_del(event_cdev);
		unregister_chrdev_region(event_dev,COUNT);
		return ret;
	}

    event_class = class_create(DEVICE_NAME);
    if(IS_ERR(event_class)){
    	ret = PTR_ERR(event_class);
    	cdev_del(event_cdev);
    	unregister_chrdev_region(event_dev,COUNT);
    	pr_err("Failed to create device: error code %d\n", ret);
    	return ret;

    }
    event_device = device_create(event_class,NULL,event_dev,NULL,DEVICE_NAME);
    if(IS_ERR(event_device)){
    	ret = PTR_ERR(event_device);
         class_destroy(event_class);
         cdev_del(event_cdev);
         unregister_chrdev_region(event_dev,COUNT);
         pr_err("Failed to create device: error code %d\n", ret);
         return ret;
    }
	pr_info("Module init done...\n");
	pr_info("Device Name : %s\n",DEVICE_NAME);
	pr_info("major = %d, minor = %d\n",MAJOR(event_dev),MINOR(event_dev));
	return 0;
}
static void __exit eventExit(void){
         device_destroy(event_class,event_dev);
         class_destroy(event_class);
         cdev_del(event_cdev);
         unregister_chrdev_region(event_dev,COUNT);
	     pr_info("Module exited...\n");
}

module_init(eventInit);
module_exit(eventExit);