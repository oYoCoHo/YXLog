#ifndef DEMO_H
#define DEMO_H
#include "AGR_JC1_SDK_API.h"

int decode_file(void *stDec,USER_Ctrl_dec dec_Ctrl);
int encode_file();
int playAudio(char* file);
extern int run_count;
#endif // DEMO_H
