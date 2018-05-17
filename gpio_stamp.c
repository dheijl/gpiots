/***************************************************************************

 Raspberry Pi GPIO interrupt time stamping kernel module and device driver.

 Copyright (c) 2018 Danny Heijl

 With many thanks to 
 
    Derek Molloy (http://derekmolloy.ie/writing-a-linux-kernel-module-part-1-introduction/)
  and
    Christphe Blaess ( https://www.blaess.fr/christophe/2014/01/22/gpio-du-raspberry-pi-mesure-de-frequence/)

 for their inspiring and excellent articles on the subject of GPIO and Linux kernel modules


Licensed under The MIT License (MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


***************************************************************************/


#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/errno.h>

#include "gpio_fifo.h"

// ------------------ Default values ----------------------------------------

#define GPIO_TS_CLASS_NAME "gpiots"       // device class name
#define GPIO_TS_ENTRIES_NAME "gpiots%d"   // device name template
#define GPIO_TS_NB_ENTRIES_MAX 17 // number of GPIOs on R-Pi P1 header.
#define GPIO_TS_FIFO_SIZE 128     // size of FIFO timestamp buffer for each GPIO interrupt 


// ------------------- Device Info structure --------------------------------
struct gpio_ts_devinfo {
    gpio_fifo_t *fifo;                  // the FIFO buffer that stores the interrupt timestamps
    spinlock_t spinlock;                // spinlock for protecting FIFO access
    wait_queue_head_t waitqueue;        // the waitqueue for poll() support
    int opencount;                      // to ensure exclusive access to each GPIO device
};

// ------------------irq handler prototype----------------------------------

static irqreturn_t gpio_ts_handler(int irq, void *devt);


//------------------- Module parameters -------------------------------------

// the table with the requested GPIO pin numbers
static int gpio_ts_table[GPIO_TS_NB_ENTRIES_MAX];
// the number of gpio pins requested
static int gpio_ts_nb_gpios;
// the module parameters definition
module_param_array_named(gpios, gpio_ts_table, int, &gpio_ts_nb_gpios, 0644);

// ------------------ Driver private data type ------------------------------

// the irq's assigned to the gpios
static int irq_numbers[GPIO_TS_NB_ENTRIES_MAX];
// the device info table
static struct gpio_ts_devinfo *devtable[GPIO_TS_NB_ENTRIES_MAX];
// global flag to block irq handler on module unload
static bool module_unload = false;

// ------------------ Driver private methods -------------------------------

//
// open the GPIO device, ensuring exclusive access
// clear the fifo buffer
// and store the devinfo struct in the private file data
//
static int gpio_ts_open(struct inode *ind, struct file *filp) {

    int gpio_index = iminor(ind);
    struct gpio_ts_devinfo *devinfo = devtable[gpio_index];
    // ensure exclusive access
    if (devinfo->opencount > 0) {
        return -EBUSY;
    }
    gpio_fifo_clear(devinfo->fifo);    
    devinfo->opencount++;
    filp->private_data = devinfo;

    return 0;
}

//
// close the GPIO device: remove the devinfo struct from the file private data
// 
static int gpio_ts_release(struct inode *ind, struct file *filp) {

    int gpio_index = iminor(ind);
    devtable[gpio_index]->opencount--;
    filp->private_data = NULL;

    return 0;
}

//
// read timestamps from the FIFO buffer, if any
//
static ssize_t gpio_ts_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {

    int nread;
    ssize_t lg;
    int err;
    struct timespec *kbuffer;
    unsigned long irqmsk;

    struct gpio_ts_devinfo *devinfo = filp->private_data;
    kbuffer = kmalloc(length * sizeof(struct timespec), GFP_KERNEL);
    if (kbuffer == NULL)
        return -ENOMEM;
    spin_lock_irqsave(&devinfo->spinlock, irqmsk);
    nread = gpio_fifo_read(devinfo->fifo, kbuffer, length);
    spin_unlock_irqrestore(&devinfo->spinlock, irqmsk);
    if (nread > 0) {
        lg = nread * sizeof(struct timespec);
        err = copy_to_user(buffer, kbuffer, lg);
        if (err != 0)
            return -EFAULT;
    }
    kfree(kbuffer);
    return nread;
}

//
// poll support: called when the user calls poll() on an open GPIO file, or when woken up
// by the kernel following a waitqueue wake_up by the ISR
//
static unsigned int gpio_ts_poll(struct file *filp, struct poll_table_struct *polltable) {

    bool have_data;
    unsigned long irqmsk;
    struct gpio_ts_devinfo *devinfo;

    // first check if we have data waiting, return the appropriate mask if we do
    devinfo = filp->private_data;
    spin_lock_irqsave(&devinfo->spinlock, irqmsk);
    have_data = gpio_fifo_data_available(devinfo->fifo);
    spin_unlock_irqrestore(&devinfo->spinlock, irqmsk);
    // we have data, return the appropriate mask
    if (have_data) {
        return POLLPRI | POLLIN;
    }
    // we have no data yet, put our wait queue in the kernel poll table
    // so we can wait for a wake-up from the ISR when poll will be called again by the kernel
    poll_wait(filp, &devinfo->waitqueue, polltable);
    // return a zero mask so that we'll be put to sleep waiting on the waitqueue
    return 0;
}

// ------------------ IRQ handler----------- ----------------------------

//
// handles GPIO interrupts
// ignores interrupts when no file is open for the device
// otherwise gets the current timestamp
// then stores the timestamp in the fifo queue for this device 
// and wakes up the associated waitqueue so that poll() gets woken up if it's waiting
//  
static irqreturn_t gpio_ts_handler(int irq, void *arg) {

    struct timespec timestamp;
    struct gpio_ts_devinfo *devinfo;
    int nwritten;

    if (module_unload) {
        return -IRQ_NONE; // ignore if module is unloading
    }

    // first of all get the timestamp
    getnstimeofday(&timestamp);

    // get the device info structure for this gpio from the file pointer
    // note that it's just a pointer to devtable[gpio_index]
    devinfo = (struct gpio_ts_devinfo *)arg;
    if (devinfo == NULL) {
        return -IRQ_NONE;
    }
    if (devinfo->opencount <= 0) { // ignore interrupts while nobody's listening
        return -IRQ_NONE;
    }
    // lock the data fifo and insert the timestamp
    spin_lock(&devinfo->spinlock);
    nwritten = gpio_fifo_write(devinfo->fifo, &timestamp, 1);
    spin_unlock(&devinfo->spinlock);
    if (nwritten != 1) {
        printk(KERN_ERR "GPIOTS: ISR fifo overflow\n");
    }
    wake_up(&devinfo->waitqueue);

    return IRQ_HANDLED;
}

// ------------------ Driver private global data ----------------------------

static struct file_operations gpio_ts_fops = {
    .owner = THIS_MODULE, 
    .open = gpio_ts_open, 
    .release = gpio_ts_release, 
    .read = gpio_ts_read, 
    .poll = gpio_ts_poll,
};

static dev_t gpio_ts_dev;
static struct cdev gpio_ts_cdev;
static struct class *gpio_ts_class = NULL;

// ------------------ Driver init and exit methods --------------------------

// 
// initalize the device structures for each device
// create the character devices
// create the sysfs interface
// register the ISR for each GPIO device
//
static int __init gpio_ts_init(void) {

    int err;
    int i;
    int gpio;
    int irq;
    struct gpio_ts_devinfo *devinfo;

    // zero device table 
    for (i = 0; i < GPIO_TS_NB_ENTRIES_MAX; ++i) {
        devtable[i] = NULL;
    }

    // sanity checks
    if (gpio_ts_nb_gpios < 1) {
        printk(KERN_ERR "%s: I need at least one GPIO input\n", THIS_MODULE->name);
        return -EINVAL;
    }

    for (i = 0; i < gpio_ts_nb_gpios; ++i) {
        gpio = gpio_ts_table[i];
        if (!gpio_is_valid(gpio)) {
            printk(KERN_ERR "GPIOTS: invalid gpio pin %d\n", gpio);
            return -ENODEV;
        }
    }

    // create the character devices

    err = alloc_chrdev_region(&gpio_ts_dev, 0, gpio_ts_nb_gpios, THIS_MODULE->name);
    if (err != 0) {
        printk(KERN_ERR "GPIOTS: error %d allocating chdev_region\n", err);
        return err;
    }
    printk(KERN_INFO "GPIOTS: device region allocated, major number=%x\n", gpio_ts_dev);

    gpio_ts_class = class_create(THIS_MODULE, GPIO_TS_CLASS_NAME);
    if (IS_ERR(gpio_ts_class)) {
        printk(KERN_ERR "GPIOTS: Could not create class %s\n", GPIO_TS_CLASS_NAME);
        unregister_chrdev_region(gpio_ts_dev, gpio_ts_nb_gpios);
        return -EINVAL;
    }
    printk(KERN_INFO "GPIOTS: device class created\n");

    for (i = 0; i < gpio_ts_nb_gpios; i++) {
        device_create(gpio_ts_class, NULL, MKDEV(MAJOR(gpio_ts_dev), i), NULL, GPIO_TS_ENTRIES_NAME, i);
        printk(KERN_INFO "GPIOTS: Device %d created\n", i);

        devinfo = kzalloc(sizeof(struct gpio_ts_devinfo), GFP_KERNEL);
        if (devinfo == NULL)
            return -ENOMEM;
        devinfo->fifo = gpio_fifo_create(GPIO_TS_FIFO_SIZE);
        devinfo->opencount = 0;
        spin_lock_init(&devinfo->spinlock);
        init_waitqueue_head(&devinfo->waitqueue);
        devtable[i] = devinfo;
    }

    cdev_init(&gpio_ts_cdev, &gpio_ts_fops);

    err = cdev_add(&(gpio_ts_cdev), gpio_ts_dev, gpio_ts_nb_gpios);
    if (err != 0) {
        for (i = 0; i < gpio_ts_nb_gpios; i++) {
            device_destroy(gpio_ts_class, MKDEV(MAJOR(gpio_ts_dev), i));
            devinfo = devtable[i];
            gpio_fifo_destroy(devinfo->fifo);
            kfree(devinfo);
        }
        class_destroy(gpio_ts_class);
        unregister_chrdev_region(gpio_ts_dev, gpio_ts_nb_gpios);
        return err;
    }

    // set up sysfs and irqs

    for (i = 0; i < gpio_ts_nb_gpios; ++i) {
        gpio = gpio_ts_table[i];
        gpio_request(gpio, "sysfs");
        gpio_direction_input(gpio);
        gpio_export(gpio, false);
        printk(KERN_INFO "GPIOTS: gpio %d exported to sysfs for input\n", gpio);
        irq = gpio_to_irq(gpio);
        printk(KERN_INFO "GPIOTS: gpio %d mapped to IRQ %d\n", gpio, irq);
        err = request_irq(irq, gpio_ts_handler, IRQF_SHARED | IRQF_TRIGGER_RISING, THIS_MODULE->name, devtable[i]);
        if (err != 0) {
            devinfo = devtable[i];
            gpio_fifo_destroy(devinfo->fifo);
            kfree(devinfo);
            printk(KERN_ERR "GPIOTS: request_irq returned error %d for gpio %d\n", err, gpio);
            return -ENODEV;
        }
        irq_numbers[i] = irq;
    }

    return 0;
}

//
// clean up the module
// unregister the ISR for each device
// remove sysfs interface and devices
// free the device info structures and associated fifos
//
void __exit gpio_ts_exit(void) {
    int i;
    int gpio;
    int irq;

    module_unload = true;

    // release IRQ's, clean up sysfs 
    for (i = 0; i < gpio_ts_nb_gpios; i++) {
        gpio = gpio_ts_table[i];
        irq = irq_numbers[i];
        free_irq(irq, devtable[i]);
        gpio_unexport(gpio);
        gpio_free(gpio);
        printk(KERN_INFO "GPIOTS: released gpio %d, irq %d\n", gpio, irq);
    }
    // clean up char devices
    cdev_del(&gpio_ts_cdev);

    for (i = 0; i < gpio_ts_nb_gpios; i++)
        device_destroy(gpio_ts_class, MKDEV(MAJOR(gpio_ts_dev), i));

    class_destroy(gpio_ts_class);
    gpio_ts_class = NULL;

    unregister_chrdev_region(gpio_ts_dev, gpio_ts_nb_gpios);

    // and finally release device info memory
    for (i = 0; i < gpio_ts_nb_gpios; i++) {
        gpio_fifo_destroy(devtable[i]->fifo);
        kfree(devtable[i]);
    }
}

module_init(gpio_ts_init);
module_exit(gpio_ts_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("danny.heijl@telenet.be");
