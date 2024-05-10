#ifndef RRESAMPLE_H
#define RRESAMPLE_H

int init_PCM_resample(int output_channels, int input_channels, int output_rate, int input_rate);
int start_PCM_resample(short *output, short *input, int in_len);
int uninit_PCM_resample();
int bit_wide_transform(int flag,int in_len,unsigned char* in_buf,unsigned char* out_buf);
int volume_control(short* out_buf,short* in_buf,int in_len, float in_vol);


#endif // RRESAMPLE_H
