#include<linux/init.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/uaccess.h>
#include<linux/string.h>
#include<linux/ktime.h>
#include<linux/wait.h>
#include<linux/spinlock.h>
#include<linux/workqueue.h>
#include<linux/kthread.h>
#include<linux/ioctl.h>
#include<linux/sched.h>
#include<linux/interrupt.h>
#include<linux/gpio/consumer.h>
#include<linux/gpio/machine.h>

#define MAGIC_NUM    'k'
#define GET_TOTAL_EVENT_LOGGED  _IOR(MAGIC_NUM,1,uint64_t)
#define GET_RING_BUF_COUNT      _IOR(MAGIC_NUM,2,uint64_t)

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EVENT LOGGER PROJECT - GPIO interrupt driven");
MODULE_AUTHOR("JAYARAJ");

/*------------Macros-------------------------------*/
#define BASEMINOR    0
#define COUNT        1
#define DEVICE_NAME  "eventlogger"
#define CAPACITY     100
#define GPIO_LINE    17
/***************************************************/

/*------------structure declaration----------------*/
static dev_t event_dev;
static struct cdev *event_cdev;
static struct class *event_class;
static struct device *event_device;

static struct work_struct event_work;
static struct task_struct *thread;

static spinlock_t lock;
static wait_queue_head_t event_queue;

static struct gpio_desc *button;
static unsigned int irq;

static struct gpiod_lookup_table gpios_table = {
    .dev_id = DEVICE_NAME,
    .table = {
        GPIO_LOOKUP("pinctrl-rp1", GPIO_LINE, "event-button", GPIO_ACTIVE_LOW),
        {}
    },
};

typedef struct {
    ktime_t time;
} timestamp;

typedef struct {
    timestamp arr[CAPACITY];
    uint8_t head;
    uint8_t tail;
    uint64_t count;
} event_buf;

static event_buf ring_buf;
static uint64_t total_events_logged = 0;
static ktime_t pending_time;
/**************************************************/

/*-----------function prototypes----------------------*/
static int event_open(struct inode *inode, struct file *filp);
static int event_release(struct inode *inode, struct file *filp);
static void event_work_handler(struct work_struct *work);
static int event_stats_thread(void *data);
static void init_stats_thread(void);
static irqreturn_t button_irq_handler(int irq, void *dev_id);
/*****************************************************/

static int event_open(struct inode *inode, struct file *filp) {
    pr_info("eventlogger: device opened by process \"%s\" (pid %d)\n",
            current->comm, current->pid);
    return 0;
}

static int event_release(struct inode *inode, struct file *filp) {
    pr_info("eventlogger: device closed by process \"%s\" (pid %d)\n",
            current->comm, current->pid);
    return 0;
}

static ssize_t timestamp_read(struct file *filp, char __user *buf, size_t count, loff_t *offset) {
    char kbuf[64];
    int len;
    ktime_t event_time;
    unsigned long flags;
    u64 event_number;
    int ret;

    ret = wait_event_interruptible(event_queue, ring_buf.count > 0);
    if (ret)
        return ret;

    spin_lock_irqsave(&lock, flags);
    if (ring_buf.count == 0) {
        spin_unlock_irqrestore(&lock, flags);
        return -EAGAIN;
    }

    event_time = ring_buf.arr[ring_buf.tail].time;
    event_number = total_events_logged - ring_buf.count + 1;
    ring_buf.tail = (ring_buf.tail + 1) % CAPACITY;
    ring_buf.count--;
    spin_unlock_irqrestore(&lock, flags);

    len = snprintf(kbuf, sizeof(kbuf), "Event:%llu,timestamp:%lld\n",
                   event_number, (long long)event_time);
    if (len > count)
        len = count;

    if (copy_to_user(buf, kbuf, len)) {
        pr_err("eventlogger: copy_to_user failed while returning event #%llu\n", event_number);
        return -EFAULT;
    }

    return len;
}

static long event_ioctl_handler(struct file *filp, unsigned int cmd, unsigned long arg) {
    uint64_t total_event;
    uint64_t total_buf_count;
    unsigned long flags;

    spin_lock_irqsave(&lock, flags);
    total_event = total_events_logged;
    total_buf_count = ring_buf.count;
    spin_unlock_irqrestore(&lock, flags);

    switch (cmd) {
    case GET_RING_BUF_COUNT:
        if (copy_to_user((uint64_t __user *)arg, &total_buf_count, sizeof(total_buf_count))) {
            pr_err("eventlogger: ioctl GET_RING_BUF_COUNT failed to copy result to userspace\n");
            return -EFAULT;
        }
        break;
    case GET_TOTAL_EVENT_LOGGED:
        if (copy_to_user((uint64_t __user *)arg, &total_event, sizeof(total_event))) {
            pr_err("eventlogger: ioctl GET_TOTAL_EVENT_LOGGED failed to copy result to userspace\n");
            return -EFAULT;
        }
        break;
    default:
        pr_warn("eventlogger: received unknown ioctl command 0x%x\n", cmd);
        return -ENOTTY;
    }
    return 0;
}

static struct file_operations event_fops = {
    .open = event_open,
    .read = timestamp_read,
    .release = event_release,
    .unlocked_ioctl = event_ioctl_handler,
};

static irqreturn_t button_irq_handler(int irq, void *dev_id) {
    pending_time = ktime_get();
    schedule_work(&event_work);
    return IRQ_HANDLED;
}

static void event_work_handler(struct work_struct *work) {
    unsigned long flags;

    spin_lock_irqsave(&lock, flags);
    if (ring_buf.count >= CAPACITY) {
        spin_unlock_irqrestore(&lock, flags);
        pr_warn("eventlogger: ring buffer full (%d entries) — dropping newest event\n", CAPACITY);
        return;
    }

    ring_buf.arr[ring_buf.head].time = pending_time;
    ring_buf.count++;
    ring_buf.head = (ring_buf.head + 1) % CAPACITY;
    total_events_logged++;
    spin_unlock_irqrestore(&lock, flags);

    wake_up_interruptible(&event_queue);
    pr_info("eventlogger: event #%llu stored, %llu currently buffered\n",
            total_events_logged, ring_buf.count);
}

static int event_stats_thread(void *data) {
    unsigned long flags;
    uint64_t logged, buffered;

    while (!kthread_should_stop()) {
        spin_lock_irqsave(&lock, flags);
        logged = total_events_logged;
        buffered = ring_buf.count;
        spin_unlock_irqrestore(&lock, flags);

        pr_info("eventlogger: %llu events logged, %llu currently buffered\n",
                logged, buffered);
        schedule_timeout_interruptible(msecs_to_jiffies(15000));
    }
    pr_info("eventlogger: stats thread stopping\n");
    return 0;
}

static void init_stats_thread(void) {
    thread = kthread_create(event_stats_thread, NULL, "eventlogger_stats");
    if (thread) {
        wake_up_process(thread);
    } else {
        pr_err("eventlogger: failed to start background stats thread\n");
    }
}

static int __init eventInit(void) {
    int ret = 0;

    ret = alloc_chrdev_region(&event_dev, BASEMINOR, COUNT, DEVICE_NAME);
    if (ret < 0) {
        pr_err("eventlogger: failed to allocate a character device region: %d\n", ret);
        return ret;
    }

    event_cdev = cdev_alloc();
    if (!event_cdev) {
        pr_err("eventlogger: failed to allocate cdev structure (out of memory)\n");
        unregister_chrdev_region(event_dev, COUNT);
        return -ENOMEM;
    }

    event_cdev->ops = &event_fops;
    event_cdev->owner = THIS_MODULE;

    ret = cdev_add(event_cdev, event_dev, COUNT);
    if (ret < 0) {
        pr_err("eventlogger: failed to add cdev to the kernel: %d\n", ret);
        cdev_del(event_cdev);
        unregister_chrdev_region(event_dev, COUNT);
        return ret;
    }

    event_class = class_create(DEVICE_NAME);
    if (IS_ERR(event_class)) {
        ret = PTR_ERR(event_class);
        pr_err("eventlogger: failed to create device class: %d\n", ret);
        cdev_del(event_cdev);
        unregister_chrdev_region(event_dev, COUNT);
        return ret;
    }

    event_device = device_create(event_class, NULL, event_dev, NULL, DEVICE_NAME);
    if (IS_ERR(event_device)) {
        ret = PTR_ERR(event_device);
        pr_err("eventlogger: failed to create /dev/%s: %d\n", DEVICE_NAME, ret);
        class_destroy(event_class);
        cdev_del(event_cdev);
        unregister_chrdev_region(event_dev, COUNT);
        return ret;
    }

    gpiod_add_lookup_table(&gpios_table);

    button = gpiod_get(event_device, "event-button", GPIOD_IN);
    if (IS_ERR(button)) {
        ret = PTR_ERR(button);
        pr_err("eventlogger: failed to acquire GPIO%d descriptor: %d (check wiring / lookup table)\n",
               GPIO_LINE, ret);
        gpiod_remove_lookup_table(&gpios_table);
        device_destroy(event_class, event_dev);
        class_destroy(event_class);
        cdev_del(event_cdev);
        unregister_chrdev_region(event_dev, COUNT);
        return ret;
    }

    ret = gpiod_direction_input(button);
    irq = gpiod_to_irq(button);
    if (ret < 0 || irq < 0) {
        pr_err("eventlogger: failed to configure GPIO%d as interrupt input (dir_ret=%d, irq=%d)\n",
               GPIO_LINE, ret, irq);
        gpiod_put(button);
        gpiod_remove_lookup_table(&gpios_table);
        device_destroy(event_class, event_dev);
        class_destroy(event_class);
        cdev_del(event_cdev);
        unregister_chrdev_region(event_dev, COUNT);
        return (ret < 0) ? ret : irq;
    }

    ret = request_irq(irq, button_irq_handler, IRQF_TRIGGER_FALLING,
                       "event-button", DEVICE_NAME);
    if (ret) {
        pr_err("eventlogger: failed to register IRQ %u for GPIO%d: %d\n", irq, GPIO_LINE, ret);
        gpiod_put(button);
        gpiod_remove_lookup_table(&gpios_table);
        device_destroy(event_class, event_dev);
        class_destroy(event_class);
        cdev_del(event_cdev);
        unregister_chrdev_region(event_dev, COUNT);
        return ret;
    }

    spin_lock_init(&lock);
    INIT_WORK(&event_work, event_work_handler);
    init_waitqueue_head(&event_queue);
    init_stats_thread();

    pr_info("eventlogger: ready — /dev/%s (major=%d, minor=%d), listening on GPIO%d via IRQ %u\n",
            DEVICE_NAME, MAJOR(event_dev), MINOR(event_dev), GPIO_LINE, irq);
    return 0;
}

static void __exit eventExit(void) {
    free_irq(irq, DEVICE_NAME);
    gpiod_put(button);
    gpiod_remove_lookup_table(&gpios_table);

    kthread_stop(thread);
    cancel_work_sync(&event_work);

    device_destroy(event_class, event_dev);
    class_destroy(event_class);
    cdev_del(event_cdev);
    unregister_chrdev_region(event_dev, COUNT);

    pr_info("eventlogger: unloaded, %llu events were logged this session\n", total_events_logged);
}

module_init(eventInit);
module_exit(eventExit);