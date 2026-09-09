#include<linux/init.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/uaccess.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EVENT LOGGER PROJECT");
MODULE_AUTHOR("JAYARAJ");


/*------------Macros-------------------------------*/
#define BASEMINOR    0
#define COUNT        1
#define DEVICE_NAME  "eventlogger"

#define BUFFER_SIZE   100
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

/*-----------device private structures------------*/
typedef struct{
	char data[BUFFER_SIZE];
	size_t length;
}event_dev_data;

static event_dev_data event_data;

/**************************************************/


static int event_open(struct inode *inode, struct file *filp) {
    pr_info("File opened successfully...\n");

    // Moved lines 44 & 45 here so they execute cleanly
    //strscpy(event_data.data, "hello world", sizeof(event_data.data));
    //event_data.length = strlen(event_data.data);

    return 0;
}
static ssize_t event_read(struct file *filp,char __user *buf,size_t count,loff_t *offset){
	size_t bytes_available = 0;
    if(filp == NULL){
    	pr_err("%s:%d, File pointer is not valid\n",__func__,__LINE__);
    	return -EINVAL;
    }
    if(buf == NULL){
    	pr_err("%s:%d, Invalid user buffer (NULL)\n",__func__,__LINE__);
    	return -EINVAL;
    }
    if(count <= 0){
    	 pr_err("%s:%d, Invalid Count value\n",__func__,__LINE__);
    	 return -EINVAL;
    }
    if(offset == NULL){
    	pr_err("%s:%d, Offset pointer is invalid\n",__func__,__LINE__);
    	return -EINVAL;
    }
    if(*offset > BUFFER_SIZE){
    	return 0;
    }
    bytes_available = event_data.length - (*offset);
    pr_debug("%s:%d bytes_available = %ld\n",__func__,__LINE__,bytes_available);
    pr_debug("%s:%d read byte count = %ld\n",__func__,__LINE__,count);
    pr_debug("%s:%d read offset = %lld\n",__func__,__LINE__,*offset);
    if(count > bytes_available){
    	   count = bytes_available;
    }
    if(copy_to_user(buf,event_data.data + *offset,count)){
    	    pr_err("%s:%d, copy_to_user Failed\n",__func__,__LINE__);
    	    return -EFAULT;
    }
    *offset+=count;

    return count;





}
static ssize_t event_write(struct file *filp,const char __user *buf,size_t count,loff_t *offset){
    size_t space_available = 0;
    if(filp == NULL){
    	pr_err("%s:%d, File pointer is not valid\n",__func__,__LINE__);
    	return -EINVAL;
    }
    if(buf == NULL){
    	pr_err("%s:%d, Invalid user buffer (NULL)\n",__func__,__LINE__);
    	return -EINVAL;
    }
    if(count <= 0){
    	 pr_err("%s:%d, Invalid Count value\n",__func__,__LINE__);
    	 return -EINVAL;
    }
    if(offset == NULL){
    	pr_err("%s:%d, Offset pointer is invalid\n",__func__,__LINE__);
    	return -EINVAL;
    }
    if(*offset > BUFFER_SIZE){
    	return -ENOSPC;
    }
    space_available = BUFFER_SIZE - (*offset);
    pr_debug("%s:%d space_available = %ld\n",__func__,__LINE__,space_available);
    pr_debug("%s:%d write byte count = %ld\n",__func__,__LINE__,count);
    pr_debug("%s:%d write offset = %lld\n",__func__,__LINE__,*offset);
    if(count > space_available){
    	   count = space_available;
    }
    if(copy_from_user(event_data.data + *offset,buf,count)){
    	    pr_err("%s:%d, copy_from_user Failed\n",__func__,__LINE__);
    	    return -EFAULT;
    }
    *offset+=count;
    event_data.length = *offset;

    return count;
}
static int event_release(struct inode *inode,struct file *filp){
	pr_info("File closed successfully...");
	return 0;
}
static struct file_operations event_fops = {
	.open = event_open,
	.read= event_read,
	.write = event_write,
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