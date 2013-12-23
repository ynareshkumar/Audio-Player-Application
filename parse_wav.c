#include "parse_wav.h"

/*microblaze is big endian but wave files are strore
  using little endian so we must reverse byte order*/
void reverse_endian(void *in_ptr, void *out_ptr, int num_bytes)
{
    int i; //loop variable
    unsigned char* in = in_ptr;
    unsigned char* out = out_ptr;

    /*loop over number of bytes*/
    for(i=0; i<num_bytes; i++) 
        out[i] = in[(num_bytes-1)-i];
}
    
/*Extract wave file properties given by *buff.
  Return -1 if the file is not of proper 
  format.*/
int parse_wav(unsigned char *buff, wav_properties_t *wav)
{
      /*strings used for file format checks*/
      char riff_str[5] = "RIFF";
      char wave_str[5] = "WAVE";
      char fmt_str[5] = "fmt ";
      char data_str[5] = "data";
      /*points to the next byte in the array*/
      unsigned char* cur_ptr = buff;
      /*temporary variables used to hold fields within file*/
      unsigned int length, byte_rate, tmp;
      unsigned int sample_rate, num_bytes;
      unsigned short format, num_channels,bits_per_sample;


      /*ensure curid wave file*/
      /*the first 4 bytes of wave file should
      contain "RIFF"*/
      if(strncmp((char*)cur_ptr,riff_str,4)!=0)
           return -1;
      cur_ptr = cur_ptr + 4;
      /*the next 4 bytes contains the length
        of the rest of the file*/
#ifdef isBigEndian
        reverse_endian(cur_ptr,&length, 4);
#else
        length =*(unsigned int*) cur_ptr;
#endif
#ifdef printInfo
      printf("length of file %d\n",length);
#endif
      cur_ptr=cur_ptr+4;
      /*next 4 bytes should contain "WAVE"*/
      if(strncmp((char*)cur_ptr,wave_str,4)!=0)
           return -1;
      cur_ptr = cur_ptr + 4;
      /*next 4 bytes should contain "fmt "*/
      if(strncmp((char*)cur_ptr,fmt_str,4)!=0)
           return -1;
      cur_ptr = cur_ptr + 8;
      /*format check complete*/
       
      /*format field*/
#ifdef isBigEndian
        reverse_endian(cur_ptr,&format,2);
#else
        format =*(unsigned short*)cur_ptr;
#endif

#ifdef printInfo
      printf("format = %d\n",format);
#endif
      cur_ptr=cur_ptr+2;
      /*number of channels field*/
#ifdef isBigEndian
        reverse_endian(cur_ptr,&num_channels,2);
#else
        num_channels = *(unsigned short*)cur_ptr;
#endif
      printf("Number of channels = %d\n",num_channels);
      cur_ptr=cur_ptr+2;
      /*sampling rate field*/
#ifdef isBigEndian
        reverse_endian(cur_ptr,&sample_rate,4);
#else
        sample_rate = *(unsigned int*)cur_ptr;
#endif

#ifdef printInfo
      printf("Sampling Rate = %d\n",sample_rate);
#endif

      cur_ptr=cur_ptr+4;
      /*byte rate field*/
#ifdef isBigEndian
        reverse_endian(cur_ptr,&byte_rate,4);
#else
        byte_rate = *(unsigned int*)cur_ptr;
#endif

#ifdef printInfo
      printf("Byte Rate = %d\n",byte_rate);
#endif

      cur_ptr=cur_ptr+6;//skip block align for now
      /*bits per sample field (2 bytes)*/
#ifdef isBigEndian
        reverse_endian(cur_ptr,&bits_per_sample,2);
#else
        bits_per_sample = *(unsigned short*)cur_ptr;
#endif

#ifdef printInfo
      printf("Bits per Sample = %d\n", bits_per_sample);
#endif

      cur_ptr=cur_ptr+2;
      /*sanity check*/
      tmp = sample_rate*num_channels*bits_per_sample/8;
      if(byte_rate != tmp)
        return -1;

      /*more sanity checks*/
      /*next 4 bytes should contain "data"*/
      if(strncmp((char*)cur_ptr,data_str,4)!=0)
           return -1;
      cur_ptr = cur_ptr + 4;
 
      /*next 4 bytes contains the number of data bytes*/
#ifdef isBigEndian
        reverse_endian(cur_ptr,&num_bytes,4);
#else
        num_bytes = *(unsigned int*)cur_ptr;
#endif

#ifdef printInfo
      printf("Number of data bytes = %d\n",num_bytes);
#endif

      cur_ptr = cur_ptr + 4; //point to beginning of data

      /*place parsed values into structure*/
      wav->format = format;
      wav->num_channels = num_channels;
      wav->sample_rate = sample_rate;
      wav->bits_per_sample = bits_per_sample;
      wav->num_bytes = num_bytes;
      wav->audio_samples = cur_ptr;
      /*if we got to this point, the file has been successfully
        parsed!*/
      return(0);
}
