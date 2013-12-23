/* All of our linux kernel includes. */
#include <linux/module.h>  /* Needed by all modules */
#include <linux/moduleparam.h>  /* Needed for module parameters */
#include <linux/kernel.h>  /* Needed for printk and KERN_* */
#include <linux/init.h>	   /* Need for __init macros  */
#include <linux/ioctl.h>
#include <linux/fs.h>	   /* Provides file ops structure */
#include <linux/slab.h>    // for kmalloc & kfree
#include <linux/sched.h>   /* Provides access to the "current" process
			      task structure */
#include <asm/uaccess.h>   /* Provides utilities to bring user space
			      data into kernel space.  Note, it is
			      processor arch specific. */
#include <linux/semaphore.h>	/* Provides semaphore support */
#include <linux/wait.h>		/* For wait_event and wake_up */
#include <linux/interrupt.h>	/* Provide irq support functions (2.6
				   only) */
#include <asm/io.h>
#include "xparameters.h"
#include "xac97.h"

#define PHY_ADDR_AC97 XPAR_OPB_AC97_CONTROLLER_REF_0_BASEADDR
#define MEMSIZE_AC97 XPAR_OPB_AC97_CONTROLLER_REF_0_HIGHADDR - PHY_ADDR_AC97 + 1

/* Some defines */
#define DEVICE_NAME "audio_buf"	//Device name 
#define BUF_LEN	4096	//plackback FIFO has space for 4k
#define IRQ_NUM 1		//IRQ number for Playback peripheral

/* Function prototypes, so we can setup the function pointers for dev
   file access correctly. */
int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static int device_ioctl(struct inode *, struct file *, unsigned int, unsigned int *);
static irqreturn_t irq_handler(int irq, void *dev_id);

/* 
 * Global variables are declared as static, so are global but only
 * accessible within the file.
 */
unsigned int virt_addr_ac97;		//used for virtual memory
static int Major;		/* Major number assigned to our device driver */
static unsigned short mono;
static int Device_Open = 0;	/* Flag to signify open device */
static struct semaphore sem;  /* mutual exclusion semaphore for race
				 on file open  */
static wait_queue_head_t queue; /* wait queue used by driver for
				   blocking I/O */

/* This structure defines the function pointers to our functions for
   opening, closing, reading and writing the device file.  There are
   lots of other pointers in this structure which we are not using,
   see the whole definition in linux/fs.h */
static struct file_operations fops = {
  .read = device_read,
  .write = device_write,
  .open = device_open,
  .release = device_release,
  .ioctl = device_ioctl
};
