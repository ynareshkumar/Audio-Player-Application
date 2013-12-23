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
/* User defined header files */
#include "xparameters.h"
//#include "xac97.h"
#include "xac97.h"
#include "audio_samples.h"

//Starting address and size of addresses for AC97
#define PHY_ADDR XPAR_OPB_AC97_CONTROLLER_REF_0_BASEADDR
#define MEMSIZE XPAR_OPB_AC97_CONTROLLER_REF_0_HIGHADDR - XPAR_OPB_AC97_CONTROLLER_REF_0_BASEADDR+1

/* Some defines */
#define DEVICE_NAME "audiotest"	//Device name 
#define BUF_LEN 100		//length of a message queue
#define IRQ_NUM 1		//IRQ number for Playback peripheral

/* Function prototypes, so we can setup the function pointers for dev
   file access correctly. */
int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static int device_ioctl(struct inode *, struct file * , int ,  int *) ;
static irqreturn_t irq_handler(int irq, void *dev_id);

/* 
 * Global variables are declared as static, so are global but only
 * accessible within the file.
 */

static int Major;		/* Major number assigned to our device driver */
static int Device_Open = 0;	/* Flag to signify open device */
static struct semaphore sem;  /* mutual exclusion semaphore for race
				 on file open  */
static wait_queue_head_t queue; /* wait queue used by driver for
				   blocking I/O */
static char msg[BUF_LEN];	/* The msg the device will give when asked */

/* This structure defines the function pointers to our functions for
   opening, closing, reading and writing the device file.  There are
   lots of other pointers in this structure which we are not using,
   see the whole definition in linux/fs.h */
static struct file_operations fops = {
  .read = device_read,			//corresponding to read() function
  .write = device_write,		//corresponding to write() function
  .open = device_open,			//corresponding to open() function
  .release = device_release,	//corresponding to close() function
  .ioctl = device_ioctl,
};
