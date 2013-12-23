/*  irdemoddevice.c - Simple character device module
 *  Decodes the IR signal and provides functionality for reading the value from register to user space buffer.
 */

/* Moved all prototypes and includes into the headerfile */
#include "irdemoddevice.h"
#include "xparameters.h"

#define PHY_ADDR XPAR_IR_DEMOD_0_BASEADDR//Physical address of IR decoder
#define MEMSIZE XPAR_IR_DEMOD_0_HIGHADDR - XPAR_IR_DEMOD_0_BASEADDR+1//Size of physical address range for IR decoder

void* virt_addr;//Virtual address pointing to registers of IR decoder
char readbuf[12];
static int *msgqueue;//Queue to hold the incoming messages
/*readindex - Last index value where message is read by the application
  writeindex - Last index of the incoming value from user_logic.v
  sizeofqueue - Size of the queue based on ead and write index
  */
int readindex,writeindex,sizeofqueue;

/* This structure defines the function pointers to our functions for
   opening, closing, reading and writing the device file.  There are
   lots of other pointers in this structure which we are not using,
   see the whole definition in linux/fs.h */
static struct file_operations fops = {
  .read = device_read,
  .write = device_write,
  .open = device_open,
  .release = device_release
};

/*
 * This function is called when the module is loaded and registers a
 * device for the driver to use.
 */
int my_init(void)
{
  
  init_waitqueue_head(&queue);	/* initialize the wait queue */

  /* Initialize the semaphor we will use to protect against multiple
     users opening the device  */

  sema_init(&sem, 1);

  Major = register_chrdev(0, DEVICE_NAME, &fops);
  if (Major < 0) {		
    printk(KERN_ALERT "Registering char device failed with %d\n", Major);
    return Major;
  }
  printk(KERN_INFO "Registered a device with dynamic Major number of %d\n", Major);
  printk(KERN_INFO "Create a device file for this device with this command:\n'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);

//Map virtual address to IR decoder physical address
  virt_addr = ioremap(PHY_ADDR,MEMSIZE);  

  return 0;		/* success */
}

/*
 * This function is called when the module is unloaded, it releases
 * the device file.
 */
void my_cleanup(void)
{
  /* 
   * Unregister the device 
   */
  printk(KERN_ALERT "Unregistering the device driver\n");
  //Remove virtual address mapping in clean up
  unregister_chrdev(Major, DEVICE_NAME);
//Free the memory allocated to queue
  kfree(msgqueue);

  //Unmapping virtual meomory	
  printk(KERN_ALERT "Unmapping virtual address space\n");
  iounmap((void *)virt_addr);

}


/* 
 * Called when a process tries to open the device file, like "cat
 * /dev/irdemod".  Link to this function placed in file operations
 * structure for our device file.
 */
static int device_open(struct inode *inode, struct file *file)
{
  int irq_ret;

  if (down_interruptible (&sem))
	return -ERESTARTSYS;	

  /* We are only allowing one process to hold the device file open at
     a time. */
  if (Device_Open){
    up(&sem);
    return -EBUSY;
  }
  Device_Open++;
  
  /* OK we are now past the critical section, we can release the
     semaphore and all will be well */
  up(&sem);

//Dynamically allocate memory to queue
  msgqueue = (unsigned int *)kmalloc(BUF_LEN*sizeof(unsigned int), GFP_KERNEL);

  if(msgqueue == NULL)
  {
	printk(KERN_ALERT "Error allocating memory");
        return -1;
  }
  
  /* request a fast IRQ and set handler */
  irq_ret = request_irq(IRQ_NUM, irq_handler, 0 /*flags*/ , DEVICE_NAME, NULL);
  if (irq_ret < 0) {		/* handle errors */
    printk(KERN_ALERT "Registering IRQ failed with %d\n", irq_ret);
    return irq_ret;
  }

//Initialize values to zero
  readindex = 0;
  writeindex = 0;

  try_module_get(THIS_MODULE);	/* increment the module use count
				   (make sure this is accurate or you
				   won't be able to remove the module
				   later. */

  msg_Ptr = NULL;
  return 0;
}

/* 
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
  Device_Open--;		/* We're now ready for our next caller */
  
  free_irq(IRQ_NUM, NULL);
  
  /* 
   * Decrement the usage count, or else once you opened the file,
   * you'll never get get rid of the module.
   */	
  module_put(THIS_MODULE);
  
  return 0;
}

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
  int bytes_read = 0;
  int res;
  int numberofmessages;
  
  /* In this driver msg_Ptr is NULL until an interrupt occurs */
  wait_event_interruptible(queue, (msg_Ptr != NULL)); /* sleep until
							 interrupted */
//If readindex exceeds length, then switch back to start
  if(readindex >= BUF_LEN)
    readindex =0;

  if (writeindex < readindex)	//calculate the message size in the queue
	sizeofqueue = 100 - (readindex - writeindex);
  else
	sizeofqueue = writeindex - readindex;

  if(sizeofqueue == 0)
    return 0;
  
  
  /* 
   * Actually put the data into the buffer 
   */
  while (length > 0) {
    
    //Read 4 bytes from queue and copy to user space buffer 
    res = put_user(*(msgqueue+readindex),(int *) buffer++); 

    if(res != 0)
		return -1;
    
    length = length - 4;
    bytes_read = bytes_read + 4;
    //Increment read psoition to point to next message
    readindex++;
  }
  
  /* 
   * Most read functions return the number of bytes put into the buffer
   */
  return bytes_read;
}

/*  
 * Called when a process writes to dev file: echo "hi" > /dev/hello 
 * Next time we'll make this one do something interesting.
 */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{

  /* not allowing writes for now, just printing a message in the
     kernel logs. */
  printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
  return -EINVAL;		/* Fail */
}

irqreturn_t irq_handler(int irq, void *dev_id) {
  static int counter = 0;	/* keep track of the number of
				   interrupts handled */
  unsigned int slvreg0;
  sprintf(msg, "IRQ Num %d called, interrupts processed %d times\n", irq, counter++);
  msg_Ptr = msg;

  wake_up_interruptible(&queue);   /* Just wake up anything waiting
				      for the device */
				      
//Assign the busy bit to 1 so that slv_reg0 is not populated until value is copied				      
  iowrite32(0x80000001,virt_addr+0x8); 
//Copy slv_reg0 register value to temporary variable
  slvreg0 = ioread32(virt_addr+0x0);
//If writeindex exceeds length, then switch back to start  
  if(writeindex >= BUF_LEN)
 	writeindex = 0; 
//Copy to the queue
  msgqueue[writeindex++] = slvreg0;
//Reset the busy bit so that new value can now be populated in slv_reg0
  iowrite32(0x80000000,virt_addr+0x8); 

 return IRQ_HANDLED;
}



/* These define info that can be displayed by modinfo */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Naresh and Lee");
MODULE_DESCRIPTION("Module which creates a character device and allows user interaction with it");

/* Here we define which functions we want to use for initialization
   and cleanup */
module_init(my_init);
module_exit(my_cleanup);
