#include "audio_buffer.h"

/*
 * This function is called when the module is loaded and registers a
 * device for the driver to use.
 */
int my_init(void)
{
  //virtual memory mapping for AC97 peripheral
  unsigned long phys_addr_ac97 = PHY_ADDR_AC97;
  unsigned long size_addr_ac97 = MEMSIZE_AC97;
  printk(KERN_INFO "Mapping virtual address for AC97 peripheral...\n");
  virt_addr_ac97 = (void *)ioremap(phys_addr_ac97, size_addr_ac97);

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
  printk(KERN_ALERT "unregisterring the device driver...\n");
  unregister_chrdev(Major, DEVICE_NAME);

  //Unmapping virtual meomory	
  printk(KERN_ALERT "unmapping virtual address space...\n");
  iounmap((void *)virt_addr_ac97);
}

/* 
 * Called when a process tries to open the device file, like "cat
 * /dev/irq_test".  Link to this function placed in file operations
 * structure for our device file.
 */
static int device_open(struct inode *inode, struct file *file)
{
  int irq_ret;	//variable for interrupt handler

  /* The Device_Open flag is a global variable and hence it is
     susceptible to races on it's access.  We'll protect it with a
     semaphore.  */
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
 
  mono = 1;	//set mono to 1 when the device is open
  
  XAC97_InitAudio(virt_addr_ac97, AC97_DIGITAL_LOOPBACK); //2 = DIGITAL LOOPBACK
  /*enable vra*/
  XAC97_WriteReg(virt_addr_ac97, AC97_ExtendedAudioStat,1);	

  printk(KERN_INFO "Device (audio_buf) is ready to be used.\n");

  /* request a fast IRQ and set handler */
  irq_ret = request_irq(IRQ_NUM, irq_handler, 0 /*flags*/ , DEVICE_NAME, NULL);
  if (irq_ret < 0) {		/* handle errors */
    printk(KERN_ALERT "Registering IRQ failed with %d\n", irq_ret);
    return irq_ret;
  }

  return 0;
}

/* 
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
  Device_Open--;		/* We're now ready for our next caller */
  
  /* soft reset the AC97 peripheral */
  XAC97_SoftReset(virt_addr_ac97);
  
  free_irq(IRQ_NUM, NULL);	//unregister the interrupt handler

  printk(KERN_INFO "This device is being closed...\n");  
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
  printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
  return 4;
}

/*  
 * Called when a process writes to dev file: echo "hi" > /dev/hello 
 * Next time we'll make this one do something interesting.
 */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	unsigned char *sample_var;
	int i;
	
	//allocate memory to sample_var pointer
	sample_var = (unsigned char *)kmalloc(len*sizeof(unsigned char), GFP_KERNEL);
	if (sample_var == NULL) {	
    /* Failed to get memory, exit gracefully */
    printk(KERN_ALERT "Unable to allocate needed memory\n");

    return 10; 			/* Defining error code of 10 for
				   "Unable to allocate memory" */
  	}	
    
	//copy data from user space buffer
	copy_from_user((unsigned char *)sample_var,(unsigned char *)buff,len);

	//write data into AC97 register
	for(i = 0; i< len; i++){
		if(mono){
			iowrite32(sample_var[i],virt_addr_ac97+AC97_IN_FIFO_OFFSET);
			iowrite32(sample_var[i],virt_addr_ac97+AC97_IN_FIFO_OFFSET);
		}
		else {
			iowrite32(sample_var[i],virt_addr_ac97+AC97_IN_FIFO_OFFSET);
		}
		//block write method while it writes data into AC97 register
		wait_event_interruptible(queue, (XAC97_isInFIFOFull(virt_addr_ac97)!=1));
	}

	kfree(sample_var);	//deallocate memory 
	return i;		
}

irqreturn_t irq_handler(int irq, void *dev_id) {
  wake_up_interruptible(&queue);   /* Just wake up anything waiting
				      for the device */
  return IRQ_HANDLED;
}

/*This function allows the user process to provide control commands
to our device driver and status from the device*/
static int device_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned int *val_ptr)
{
	u16 val;	//temporary value
	get_user(val, (u16 *)val_ptr);	//grab value from user space

	/*switch statement to execute commands */
	switch(cmd)
	{
		/* adjust aux volume */
		case ADJUST_AUX_VOL:
			XAC97_WriteReg(virt_addr_ac97, AC97_AuxOutVol, val);
			printk(KERN_INFO "Device Driver: Aux volume has been changed to %x.\n", val);	
			break;

		/* adjust master volume */
		case ADJUST_MAST_VOL:
			XAC97_WriteReg(virt_addr_ac97, AC97_MasterVol, val);
			printk(KERN_INFO "Device Driver: Master volume has been changed to %x.\n", val);	
			break;

		/*adjust playback rate */
		case ADJUST_PLAYBACK_RATE:
			XAC97_WriteReg(virt_addr_ac97, AC97_PCM_DAC_Rate, val);
			printk(KERN_INFO "Device Driver: Play-rate has been changed to %hu.\n", val);
			break;

		/*enable/disable mono */
		case ENABLE_DISABLE_MONO:
			//code
			if (val){
				mono = 1;
			} else {
				mono = 0;
			}
			break;
		
		/*clear playback FIFO (queue) in AC97 audio device*/
		case CLEAR_PLAYBACK_FIFO:
			XAC97_ClearFifos(virt_addr_ac97);
			break;

		//if unknown command, error out
		default: 
			printk(KERN_INFO "Device Driver: Unsupported control command! \n");
			return -EINVAL;
	}
	return 0;
}

/* These define info that can be displayed by modinfo */
MODULE_LICENSE("ECEN 449 - 501 Lab10");
MODULE_AUTHOR("Kun mo Kim");
MODULE_DESCRIPTION("Audio buffer device");

/* Here we define which functions we want to use for initialization
   and cleanup */
module_init(my_init);
module_exit(my_cleanup);
