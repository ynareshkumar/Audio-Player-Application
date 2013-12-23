#include <stdio.h>
#include <string.h> //used for string comparisons

#define isBigEndian  //microblaze is big endian :(
#define printInfo
#define FIFO_LEN 4096

/*define a structure for holding the properties of
  the wave file*/
typedef struct {

	/*audio format: 1 for PCM otherwise 
        some type of compression is being used*/
	int format;

	/*number of audio channels
	1 for Mono, 2 for stereo*/
	int num_channels;

	/*Sampling rate is the rate at which
        the audio was recorded at and should
	be playback at this rate.
	Typical values include: 8000, 11025, 
        16000, 22050, 44100, and 48000*/
	int sample_rate;

	/*bits per sample i.e. the resolution
	of each sample.
	Could be 8 or 16*/
	int bits_per_sample;

	/*This parameter is the number of 
	bytes for the audio in the file. This
	includes all channels*/
	int num_bytes;

    /*pointer to data section within array*/
    unsigned char* audio_samples;

} wav_properties_t; 

int parse_wav(unsigned char *buff, wav_properties_t *wav_struct);
void reverse_endian(void *in, void *out, int num_bytes);
