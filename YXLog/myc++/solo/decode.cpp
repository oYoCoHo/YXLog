
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "solo.h"
#include "silk.hpp"
#ifdef _WIN32
# define SKP_STR_CASEINSENSITIVE_COMPARE(x, y) _stricmp(x, y)
#else
# define SKP_STR_CASEINSENSITIVE_COMPARE(x, y) strcasecmp(x, y)
#endif



/* PSEUDO-RANDOM GENERATOR                                                          */
/* Make sure to store the result as the seed for the next call (also in between     */
/* frames), otherwise result won't be random at all. When only using some of the    */
/* bits, take the most significant bits by right-shifting. Do not just mask off     */
/* the lowest bits.                                                                 */
#define SKP_RAND(seed)                   ((907633515) + (seed) * (196314165))


#define  cmdControl

/* Define codec specific settings */
#define MAX_BYTES_PER_FRAME     250 // Equals peak bitrate of 100 kbps
#define FRAME_LENGTH_MS         20
#define MAX_API_FS_KHZ          48
#define MAX_FRAME_SIZE 1920
#define MAX_FRAME_BYTES 1024

#define MD_NUM 2

static int rand_seed = 1;

int SilkDecoder::decode_file(vl_int8 *buff,int size,void *stDec,USER_Ctrl_dec dec_Ctrl)
{
//    qDebug("111111111111111111decode_file");
    //FILE *fin;
    //FILE *fout;
    int j;
    short FrmBuf[MAX_FRAME_SIZE];
    size_t    counter;
    int totPackets, lost = 0, i, k;
    short nBytes[6] = { 0, 0, 0, 0, 0, 0};
    unsigned char payload[MAX_FRAME_BYTES];
    unsigned char *payloadEnd = NULL, *payloadToDec = NULL;
    int loss_prob;
    int nwrite;
    int lostMD[8]={0,0,0,0,0,0};
    int lostcnt = 0;
    int MD_type = 2;
    short nSamplesOut;
    
    nwrite = dec_Ctrl.framesize_ms*dec_Ctrl.samplerate/1000;

    /* default settings */
    loss_prob = dec_Ctrl.packetLoss_perc; // Packet loss percentage  0
    memset(payload, 0, sizeof(unsigned char)*MAX_FRAME_BYTES);

    totPackets = 0;
    payloadEnd = payload;
    payloadToDec = payloadEnd;
    int a,b,c,d;

    k = 0;
    while( 1 )
    {
        

        memcpy(&nBytes[0],buff,2);
        memcpy(&nBytes[1],buff+2,2);
        memcpy(payloadEnd,buff+4,nBytes[0]);
        counter=nBytes[0];

        if (run_count % MD_NUM == 0)
        {
            /* Simulate losses */
            for (j = 0; j < MD_NUM; j++)
            {
                rand_seed = SKP_RAND(rand_seed);
                if ((((float)((rand_seed >> 16) + (1 << 15))) / 65535.0f >= (loss_prob / 100.0f)) && (counter > 0)) {
                    if(nBytes[j] == 0)
                        lostMD[j] = 1;
                    else
                        lostMD[j] = 0;
                }
                else {
                    lostMD[j] = 1;
                    lostcnt++;
                }
            }
        }

#define _SIMU1_
        if (run_count % MD_NUM == 0)
        {
            if ((lostMD[0] == 0) && (lostMD[1] == 0))
            {
                lost = 0;
                payloadToDec = payload;
                MD_type = 2;
            }
            else if ((lostMD[0] == 0) && (lostMD[1] == 1))
            {

#ifdef _SIMU1_
                lost = 0;
                payloadToDec = payload;
#ifdef _MD1_WITH_HB_
                memmove(payload + nBytes[0] - nBytes[1], payload + nBytes[0] - 4, 4 * sizeof(unsigned char));
                nBytes[0] = nBytes[0] - nBytes[1] + 4;
#else
                nBytes[0] = nBytes[0] - nBytes[1];
#endif
                nBytes[1] = 0;
                MD_type = 0;
#else
                lost = 0;
                payloadToDec = payload;
                MD_type = 2;
#endif
            }
            else if ((lostMD[0] == 1) && (lostMD[1] == 0))
            {/* Loss the MD1 Steam ,use the MD2 Steam  */
                lost = 0;
                payloadToDec = payload + nBytes[0] - nBytes[1];
                nBytes[0] = nBytes[1];
                nBytes[1] = 0;
                MD_type = 1;
            }
            else if ((lostMD[0] == 1) && (lostMD[1] == 1))
            {
                lost = 1;
            }
        }
        else
        {
            if ((lostMD[0] == 0) && (lostMD[1] == 0))
            {
                lost = 0;
                payloadToDec = payload;
                MD_type = 2;
            }
            else if ((lostMD[0] == 0) && (lostMD[1] == 1))
            {/* Loss the MD2 Steam ,use the MD1 Steam */
                lost = 0;
                payloadToDec = payload;
#ifdef _MD1_WITH_HB_
                memmove(payload + nBytes[0] - nBytes[1], payload + nBytes[0] - 4, 4 * sizeof(unsigned char));
                nBytes[0] = nBytes[0] - nBytes[1] + 4;
#else
                nBytes[0] = nBytes[0] - nBytes[1];
#endif
                nBytes[1] = 0;
                MD_type = 0;
            }
            else if ((lostMD[0] == 1) && (lostMD[1] == 0))
            {
#ifdef _SIMU1_
                lost = 0;
                payloadToDec = payload + nBytes[0] - nBytes[1];
                nBytes[0] = nBytes[1];
                nBytes[1] = 0;
                MD_type = 1;
#else
                lost = 0;
                payloadToDec = payload;
                MD_type = 2;
#endif
            }
            else if ((lostMD[0] == 1) && (lostMD[1] == 1))
            {
                lost = 1;
            }
        }

        run_count++;
        if(run_count>10000)
            run_count = 0;
        
        a = nBytes[0];
        b = nBytes[1];
        c = lostMD[0];
        d = lostMD[1];

        if (lost == 0) {
            /* Loss: Decode enough frames to cover one packet duration */
            AGR_Sate_Decoder_Decode(stDec, FrmBuf, &nSamplesOut, payloadToDec, nBytes, (MD_type + 2));
        }
        else {
            /* Loss: Decode enough frames to cover one packet duration */

            AGR_Sate_Decoder_Decode(stDec, FrmBuf, &nSamplesOut, payloadToDec, nBytes, 1);
        }

        //int num = fwrite(FrmBuf, sizeof(short), nwrite, fout);
        memset(pcmBuff, 0, sizeof(pcmBuff)); //清空数组
        memcpy(pcmBuff,FrmBuf,nwrite*2);
//        printf("decode size  = %d\n",nwrite*2);
        totPackets++;
//        qDebug()<<a<<b<<c<<d<<"num = ";
        //fclose(fin);
        //fclose(fout);
        break;
    }



    return 0;
}//end main()
