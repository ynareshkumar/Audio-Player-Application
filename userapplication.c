#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/dir.h>
#include <sys/param.h>
#include <dirent.h>
#include <string.h>
/*user defined header files*/
#include "parse_wav.h"
#include "sound.h"

//#define BUFF_LEN 2048 //2k 16-bit samples
#define BUFF_LEN 4096 //2k 16-bit samples
#define IR_BUF_LEN 200	//buffer length for IRDEMOD

/* Keys on remote controller (for IR_DEMOD) */ 
#define POWER 0x00		//power
#define STOP 0xde		//stop
#define PAUSE 0xdc		//pause
#define PLAY 0xdf		//play
#define CH_UP 0x90		//channel up
#define CH_DN 0x91		//channel down
#define VOL_UP 0x92		//volume up
#define VOL_DN 0x93		//volume down
#define MUTE 0x94		//mute
#define FORW 0xd8		//forward
#define REVR 0xd9		//reverse
#define RATE_UP 0xF4	//play_rate up
#define RATE_DN 0xF5	//play_rate down

/*function prototypes*/
static int open_wav_file(char* filename);
static int wav_file_filter(const struct direct *entry); 

/*global variables shared by all functions in this source*/
static int fd_aud, fd_wav, fd_ir;//file designators for audio device, wav file, and IR-remote device.
static struct stat file_info;//used for getting file length needed by mmap
static unsigned short val=0;//temp variable used for ioctl calls
static unsigned short rate_select=0;//for play rate
static void *contents;//points to the beginning of the mmap region
static unsigned char *audio_ptr;//points to current audio sample
static unsigned char *samples;//points to the beginning of the audio data
static int num_samples;//number of samples in current wav file
static unsigned short  msg_buff[IR_BUF_LEN];//used to hold read IR messages
/*audio playback mode settings*/
static unsigned short vol_setting, playback_rate, mono_setting; //hold current audio settings
static int play_audio = 0;//play flag
static int completion_mode = 1; //0=stop,1=repeat,2=forward,3=reverse
static int mute_audio = 0;//mute flag

int main(void) {
	int cnt=0;
    int i; //loop variable
    int ir_read_bytes, audio_write_bytes;
    int quit = 0; //exit flag
    //counters which handle the multple message problem
    int wait_count = 0;
    struct dirent **namelist;//structure pointer for reading directory
    int num_files; //number of wav files contained in directory
    int cur_file=0; //index points to wav file currently open
    int nxt_file=0;

    /*scan current directory for wav files and provide them in alphabetical order*/
    num_files = scandir(".", &namelist, wav_file_filter, alphasort);
    if (num_files < 0) //handle errors
    {
        perror("scandir");
        return -1;
    }
    
    /*open hardware devices with proper error handling*/

    /*open ir device file*/
    //use open
	fd_ir=open("/dev/IR_DEMOD",O_RDWR);
    /*handle error opening file*/
    if(fd_ir<0){
        printf("Failed to open ir device!\n");
        return -1;
    }
        
   /*clear message queue before starting*/
    read(fd_ir,msg_buff,IR_BUF_LEN);
	
    /*open audio device file*/
    //use open
	fd_aud=open("/dev/audio_buf",O_WRONLY);
    /*handle error opening file*/
    if(fd_aud<0){
        printf("Failed to open audio device!\n");
        return -1;
    }

    /*initially open first wav file*/
    /*open and mmap file, check for valid file, and adjust codec*/
    printf("Opening %s\n", namelist[cur_file]->d_name);
    if(open_wav_file(namelist[cur_file]->d_name)<0)
	{	
		printf("Cannot open wav file!!\n");
        return -1;//if error opening
	}

    while(quit != 1) //continue until killed
    {
        /*if file selection changed load in a new file*/
        if(nxt_file != cur_file)
        {
            /*munmap current mapping*/
            //use munmap
			if (munmap(contents, file_info.st_size)<0) 
			{
				printf("Error un-mmapping the file\n");
			}
           
			/*close currently opened wav file*/
            //use close
			close(fd_wav);
			
            printf("Opening %s\n", namelist[cur_file]->d_name);
            /*open and mmap file, check for valid file, and adjust codec*/
            //use open_wave_file
			if (open_wav_file(namelist[nxt_file]->d_name) <0 ){
				printf("Cannot open wav file!! \n");
				return -1;
			}

            /*clear contents already written to audio device*/
            //use ioctl

			if(ioctl(fd_aud,CLEAR_PLAYBACK_FIFO,&val) <0 ){
				printf("ioctl() returns an error.\n ");
				return -1;
			}

			/*update current file index*/
            cur_file = nxt_file;
        }

        /*copy BUFF_LEN worth of samples from wav file to 
          audio device in repeat mode*/
        if(play_audio == 1)
        {	//write audio samples to the audio device driver in blocks
			if((audio_ptr - samples + BUFF_LEN) < num_samples) {
				audio_write_bytes=write(fd_aud,audio_ptr,BUFF_LEN);
				audio_ptr += audio_write_bytes;
			}
			else { //be sure not to overstep the bounds of mmapped region
				audio_write_bytes=write(fd_aud,audio_ptr,num_samples-(audio_ptr-samples));
				audio_ptr = samples;
			}
        }
        else //if not writting audio data
            usleep(100000); //sleep for 100ms 

		/*read in all messages in queue*/
        //use read  
		ir_read_bytes=read(fd_ir,msg_buff,IR_BUF_LEN);
		if(ir_read_bytes < 0) {
			printf("Error occured while reading IR signals !! \n");
			return -1;
		}

        /*handle messages read*/
        for(i=0; i<ir_read_bytes/2;i++) //ir_read_bytes is returned from read!
        {
            switch(msg_buff[i])
            {

                /*if vol up pressed*/
                case VOL_UP:
					if(wait_count == 0)
					{
						//may need to use counter
						wait_count=1;
					    //increase vol_setting by 0x0101
						if((vol_setting > 0x0000) && (vol_setting <= 0x1f1f)){
							vol_setting = (vol_setting >> 8);
							vol_setting = vol_setting - 1;
							vol_setting = (vol_setting << 8) | (vol_setting);
						}
						val = vol_setting;
					    //call ioctl
						if(ioctl(fd_aud,ADJUST_AUX_VOL,&val) < 0){
							printf("ioctl() returns an error.\n ");
							return -1;
						}
						printf("Volume Up is pressed !! \n");
					}
				break;

                /*if vol down pressed*/
                case VOL_DN:
					if (wait_count == 0)
					{
						//may need to use counter
						wait_count=1;
						//decrease vol_setting by 0x0101
						if(vol_setting < 0x1f1f) {
							vol_setting = (vol_setting >> 8);
							vol_setting = vol_setting + 1;
							vol_setting = (vol_setting << 8) | (vol_setting);
						}
						val = vol_setting;
						//call ioctl
						if(ioctl(fd_aud,ADJUST_AUX_VOL,&val) <0 ){
							printf("ioctl() returns an error.\n ");
							return -1;
						}
						printf("Volume Down is pressed !! \n");
					}
                break;

                /*if pause is pressed*/
                case PAUSE:
                //do something with playback flag
					play_audio=0;
					printf("Pause is pressed !! \n");
                break;

                /*if play is pressed*/
                case PLAY:
                //do something with playback flag
					play_audio=1;
					printf("Play is pressed !! \n");
                break;

                /*if stop is pressed*/
                case STOP:
                //do something with playback flag
					play_audio=0;
                //do something with audio_ptr
					audio_ptr = samples;
					printf("Stop is pressed !! \n");
                break;

                /*if channel up pressed*/
                case CH_UP:
                    if(wait_count == 0)
                    {
                        /*open next wav file in directory*/
                        nxt_file=cur_file+1;
                        if(nxt_file>=num_files)
                            nxt_file=0;//circular nature
                        wait_count = 1;//trigger a wait
						printf("Channel Up is pressed !! \n");
                    }
                    break;

                /*if channel down pressed*/
                case CH_DN:
					if(wait_count == 0)
					{
						nxt_file = cur_file-1;
						if(nxt_file<0)
							nxt_file=num_files-1;	//circular nature
						wait_count = 1;
						printf("Channel Down is pressed !! \n");
					}
                    break;
				
				/*if play-rate up pressed */
				case RATE_UP:
					rate_select++;
					if(rate_select > 5)
						rate_select = 5;
					switch(rate_select){
						case 0: val = AC97_PCM_RATE_8000_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 8000 Hz \n ");
								break;						
						case 1: val = AC97_PCM_RATE_11025_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 11025 Hz \n ");
								break;
						case 2: val = AC97_PCM_RATE_16000_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 16000 Hz \n ");
								break;
						case 3: val = AC97_PCM_RATE_22050_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 22050 Hz \n ");
								break;
						case 4: val = AC97_PCM_RATE_44100_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 44100 Hz \n ");
								break;
						case 5: val = AC97_PCM_RATE_48000_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 48000 Hz \n ");
								break;
						default: printf("Out of play-rate range !! \n");						
								break;
					}
					printf("Play-rate up is pressed !! \n");
					break;

				/*if play-rate down pressed */	
				case RATE_DN:
					if(rate_select > 0)
						rate_select--;
					switch(rate_select){
						case 0: val = AC97_PCM_RATE_8000_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 8000 Hz \n ");
								break;						
						case 1: val = AC97_PCM_RATE_11025_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 11025 Hz \n ");
								break;
						case 2: val = AC97_PCM_RATE_16000_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 16000 Hz \n ");
								break;
						case 3: val = AC97_PCM_RATE_22050_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 22050 Hz \n ");
								break;
						case 4: val = AC97_PCM_RATE_44100_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 44100 Hz \n ");
								break;
						case 5: val = AC97_PCM_RATE_48000_HZ;
								ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&val);
								printf("Play-rate is set to 48000 Hz \n ");
								break;
						default: printf("Out of play-rate range !! \n");						
								break;
					}
					printf("Play-rate down is pressed !! \n");
					break;

                /*if power button pressed*/
                case POWER:
                    quit = 1;
                    ir_read_bytes=0;//exit for loop
					printf("Power is pressed !! \n");
                    break;
                
                /*when mute pressed*/
                case MUTE: //ignore additional messages until wait count is cleared
                    if(mute_audio == 0 && wait_count == 0)
                    {
                        val = AC97_VOL_MUTE;
                        mute_audio = 1;
                    }
                    else if(wait_count == 0)
                    {
                        val = vol_setting;
                        mute_audio = 0;
                    }
                    /*update codec*/
                    if(ioctl(fd_aud,ADJUST_AUX_VOL,&val) <0 ){
						printf("ioctl() returns an error.\n ");
						return -1;
					}
                    wait_count = 1;//wait 
					printf("Mute is pressed !! \n");
                    break;
    
                //map some buttons to set the completion mode
				case FORW: 
					completion_mode = 2;
					audio_ptr += 10000;
					if(audio_ptr >= (samples+num_samples))
						audio_ptr = samples + 10000 - ((samples+num_samples)-audio_ptr);
					printf("Forward is pressed !! \n");						
					break;

				case REVR:
					completion_mode = 3;
					audio_ptr -= 10000;
					if(audio_ptr < samples)
						audio_ptr = (samples+num_samples) - 10000 - (audio_ptr - samples);
					printf("Reverse is pressed !! \n");						
					break;

                default: 
                    printf("message %d not defined!\n",msg_buff[i]);
            }
        }

            /*maintain wait scheme used to ignore multiple messages*/
            if(wait_count>4)
                wait_count = 0;
            else if(wait_count!=0)
                wait_count++;
    }
    sleep(1); //wait for audio to finish playing before exiting	
    /*un-map file from memory region*/
   //use munmap 
	if (munmap(contents, file_info.st_size)<0) 
	{
		printf("Error un-mmapping the file\n");
		/* Decide here whether to close(fd) and exit() or not. Depends... */
    }
    /*close ir device*/
    close(fd_ir);
    /*close wave file*/
    close(fd_wav);
    /*close audio device*/
    close(fd_aud);

	return 0;
}

/*this function opens a wav file, 
  maps its contents to memory, parses
  the wav header, and adjusts the audio
  codec accordingly
*/
static int open_wav_file(char* filename)
{

    wav_properties_t wav_info;

    /*open wave file*/
    fd_wav=open(filename,O_RDONLY);
    /*handle error opening file*/
    if(fd_wav <0){
        printf("Failed to wave file!\n");
        return -1;
    }
    
    /*get file info*/
    if(fstat(fd_wav,&file_info)<0)
	{
		printf("Error with fstat()!\n");
		return -1;
	}

    /*use mmap to map file to region of memory*/
    //mmap file to contents use flags for read only and shared mapping
    //use file_info.st_size for file size
	contents=mmap(NULL, file_info.st_size,PROT_READ,MAP_SHARED,fd_wav,0);
    /*ensure mmap worked properly*/
    if(contents == MAP_FAILED)
    {
        printf("Error with mmap!\n");
        return -1; 
    }

    /*advise the kernel to do read ahead*/
    madvise(contents,file_info.st_size,MADV_WILLNEED);
    
    /*get wave file info*/
    if(parse_wav((unsigned char*)contents,&wav_info)<0)
    {
        printf("Invalid file type!\n");
        return -1;
    }

    /*ensure we can play wav file*/
    if(wav_info.format != 1)
    {
        printf("Compressed wav files are not supported!\n");
        return -1;
    }
    if(wav_info.bits_per_sample != 8)
    {
        printf("Only 8-bit audio is currently supported!\n");
        return -1;
    }

    /*calculate the number of samples in file*/
    num_samples = wav_info.num_bytes;//only supporting 8-bit audio

    /*adjust audio properties based on wav file*/
    if(wav_info.num_channels == 1) {//if mono
        mono_setting=1;//set mono
	}
    else { //if stereo
        mono_setting=0;//reset mono
	}
    //use ioctl to enable or disable mono 
	if (ioctl(fd_aud,ENABLE_DISABLE_MONO,&mono_setting) < 0) {
		printf("ioctl() returns an error.\n ");
		return -1;
	}

    /*set playback rate accordingly*/
    playback_rate = wav_info.sample_rate;
    //use ioctl to set playback rate
	if (ioctl(fd_aud,ADJUST_PLAYBACK_RATE,&playback_rate) < 0) {
		printf("ioctl() returns an error.\n ");
		return -1;
	}
    
    /*initialize audio buffer pointers*/
    samples = wav_info.audio_samples;//point to beginning of data section
    audio_ptr = wav_info.audio_samples;//points to current sample

    /*set play flag to start playing file*/
    play_audio = 1; 

    return 0; //success!
}

/*function used by scandir to filter out all files except
  those with the .wav extension*/
static int wav_file_filter(const struct direct *entry) 
{
    if (strstr(entry->d_name,".wav")) //if .wav in filename
        return 1;//true
    else 
        return 0;//false
}
