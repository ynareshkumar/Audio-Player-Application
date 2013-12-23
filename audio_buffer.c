/*  audio_buffer.c - Simple character device module
 *  
 * Plays the audio from audio_samples.h file repeatedly using AC97 codec device.
 * 
 */

/* Moved all prototypes and includes into the headerfile */
#include "audio_buffer.h"

void* virt_addr;//Virtual address mapping
int i;//index element in array
static int audiovalue;//Value of each sample from audio_samples.h

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

//Register the character device
  Major = register_chrdev(0, DEVICE_NAME, &fops);
//Handle alerts
  if (Major < 0) {		
    printk(KERN_ALERT "Registering char device failed with %d\n", Major);
    return Major;
  }
  printk(KERN_INFO "Registered a device with dynamic Major number of %d\n", Major);
  printk(KERN_INFO "Create a device file for this device with this command:\n'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);

//Map the virtual address
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
  unregister_chrdev(Major, DEVICE_NAME);

//Clear the FIFO buffer
  XAC97_ClearFifos((unsigned int)virt_addr);
//Reset the audio codec  
  XAC97_SoftReset((unsigned int)virt_addr);

  //Unmapping virtual memory	
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

  printk(KERN_ALERT "Inside device open \n");
  int irq_ret;

  if (down_interruptible (&sem))
	return -ERESTARTSYS;	

		printk(KERN_ALERT "after down interruptible \n");
  /* We are only allowing one process to hold the device file open at
     a time. */
  if (Device_Open){
    up(&sem);
	 	printk(KERN_ALERT "Device busy \n");
    return -EBUSY;
  }
  Device_Open++;
  
  /* OK we are now past the critical section, we can release the
     semaphore and all will be well */
  up(&sem);

	printk(KERN_ALERT "Semaphore released \n");
  i = 0;
  //Initialize the audio codec
  XAC97_InitAudio((unsigned int)virt_addr,0);
  printk(KERN_ALERT "Initialization okay \n");
  //Set the DAC rate to 11025 Hz
  XAC97_WriteReg((unsigned int)virt_addr,AC97_PCM_DAC_Rate, AC97_PCM_RATE_11025_HZ);
  printk(KERN_ALERT "Device opened successfully \n");
  //Keep the AUX and Master volume to Max to hear the audio clearly.
  XAC97_WriteReg((unsigned int)virt_addr, AC97_MasterVol, AC97_VOL_MAX);
  XAC97_WriteReg((unsigned int)virt_addr, AC97_AuxOutVol, AC97_VOL_MAX);
  
  /* request a fast IRQ and set handler */
  irq_ret = request_irq(IRQ_NUM, irq_handler, 0 /*flags*/ , DEVICE_NAME, NULL);
  if (irq_ret < 0) {		/* handle errors */
    printk(KERN_ALERT "Registering IRQ failed with %d\n", irq_ret);
    return irq_ret;
  }
  


  try_module_get(THIS_MODULE);	/* increment the module use count
				   (make sure this is accurate or you
				   won't be able to remove the module
				   later. */

  return 0;
}

/* 
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
  Device_Open--;		/* We're now ready for our next caller */
  //Release the interrupt 
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
 * read from it. This operation is not supported.
 */
static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
  /* not allowing writes for now, just printing a message in the
     kernel logs. */
  printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
  return -EINVAL;		/* Fail */
}

/*  
 * Called when a process writes to dev file. 
 *This operation is not supported.
 */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{

  /* not allowing writes for now, just printing a message in the
     kernel logs. */
  printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
  return -EINVAL;		/* Fail */
}

//Interrupt handler
irqreturn_t irq_handler(int irq, void *dev_id) {
  static int counter = 0;	/* keep track of the number of
				   interrupts handled */
  sprintf(msg, "IRQ Num %d called, interrupts processed %d times\n", irq, counter++);

 //Check if FIFO buffer is not full. Then write value to FIFO buffer
  while(XAC97_isInFIFOFull((unsigned int)virt_addr) == 0)
  {
	  //When the array reaches the end, bring back to the first element.
		if(i == NUM_SAMPLES - 1)
		{
			i = 0;
		}
		//Get the current audio sample value
		audiovalue = audio_samples[i++];
		//Write the value to buffer
		iowrite32(audiovalue,virt_addr+AC97_IN_FIFO_OFFSET);
		//printk(KERN_ALERT "FIFO Value %d\n", ioread32(virt_addr+AC97_IN_FIFO_OFFSET));
  
  
  }
  
   wake_up_interruptible(&queue);   /* Just wake up anything waiting
				      for the device */

 return IRQ_HANDLED;
}


//Function which handles the request from the user
static int device_ioctl(struct inode *inode, struct file *file,int cmd, int *val_ptr)
{
	u16 val;//temporary value
	get_user(val,(u16*)val_ptr);//grab value from user space
	printk(KERN_INFO "\n Value is %hu",val);
	// switch statement to execute commands 
	switch(cmd)
	{
	    //adjust aux volume 
		case ADJUST_AUX_VOL:
			//If the value is greater than acceptable value, then set to MAX value.
		   if(val > 0xffff || val < 0)
			XAC97_WriteReg(virt_addr, AC97_AuxOutVol, AC97_VOL_MAX);
			//Write the value entered to Master volume register	
		   else	
		  	XAC97_WriteReg(virt_addr, AC97_AuxOutVol, val);	
		  break;
	    //Adjust Master Volume
		case ADJUST_MAST_VOL:
			//If the value is greater than acceptable value, then set to MAX value.
		  if(val > 0xffff || val < 0)
			XAC97_WriteReg(virt_addr,AC97_MasterVol, AC97_VOL_MAX);
		//Write the value entered to Master volume register	
		  else
		  XAC97_WriteReg(virt_addr,AC97_MasterVol, val);
		  break;
	    //Adjust playback rate
		case ADJUST_PLAYBACK_RATE:
			//Valid playback rates are 8000,11025,16000,22050,44100,48000 Hz.
		  if(val == 1)
			 XAC97_WriteReg((unsigned int)virt_addr,AC97_PCM_DAC_Rate, AC97_PCM_RATE_8000_HZ);
		  else if(val == 2)
			 XAC97_WriteReg((unsigned int)virt_addr,AC97_PCM_DAC_Rate, AC97_PCM_RATE_11025_HZ);
		  else if(val == 3)
			 XAC97_WriteReg((unsigned int)virt_addr,AC97_PCM_DAC_Rate, AC97_PCM_RATE_16000_HZ);
		  else if(val == 4)
			 XAC97_WriteReg((unsigned int)virt_addr,AC97_PCM_DAC_Rate, AC97_PCM_RATE_22050_HZ);
		  else if(val == 5)
			 XAC97_WriteReg((unsigned int)virt_addr,AC97_PCM_DAC_Rate, AC97_PCM_RATE_44100_HZ);
		  else if(val == 6)
			 XAC97_WriteReg((unsigned int)virt_addr,AC97_PCM_DAC_Rate, AC97_PCM_RATE_48000_HZ);
		  else
			printk(KERN_INFO "Standard playback rate not found\n");
		  break;
	    //If unknown command, error out
		default:
			printk(KERN_INFO "Unsupported control command!\n");
			return -EINVAL;
	}

	return 0;

}
/* These define info that can be displayed by modinfo */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Naresh and Lee");
MODULE_DESCRIPTION("Module which creates a character device and allows user interaction with it");

/* Here we define which functions we want to use for initialization
   and cleanup */
module_init(my_init);
module_exit(my_cleanup);
