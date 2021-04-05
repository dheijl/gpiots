# gpiots

A Linux kernel module for timestamping GPIO interrupts
including a character device driver for reading the timestamped interrupts

Reading the GPIO interrupt timestamps is somewhat peculiar:

- read() is non-blocking: if no interrupts have occurred you simply get a zero return
- by default you do not read characters, you read timestamps: the length parameter in read() specifies the number of timestamps you want to read. So your buffer size must be a multiple of sizeof(timespec64), which is normally 12 bytes on 32-bit architecture, and 16 bytes on 64 bit architectures.  
When you us the `safemode=1` parameter on module installation the length parameter in read() specifies the number of bytes to read. In this mode the length parameter must be a multiple of sizeof(timespec64). The safe mode makes it possible to access the kernel module interface from environments like Python.
- by default read() returns the number of timespec structs read, not the number of bytes.  
When `safemode` is active the number of bytes read is returned.
- you should use poll() before you try to read() if you want to avoid reading in a loop until GPIO interrupts arrive
- if no gpiots*x* device is open, GPIO interrupts for that GPIO are ignored and are not buffered
- the default fifo buffer size in the kernel module is 128 timespec structs for each GPIO, but you can change this default by modifying the following define in the source of *gpio_stamp.c*:

`
#define GPIO_TS_FIFO_SIZE 128     // size of FIFO timestamp buffer for each GPIO interrupt 
`

- if the fifo buffer overflows the driver will log it, but otherwise you'll never know
- the module has an array parameter on install: `gpios=1,2,...` which lists the GPIO pins you want to monitor

