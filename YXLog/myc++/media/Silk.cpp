#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
//#include "solo.h"
#include "SKP_Silk_SDK_API.h"


#include "vl_types.h"
#include "vl_const.h"
#include "Silk.hpp"
#include "CodecManager.hpp"
#include "AudioCodec.hpp"
#include <printf.h>
#include "AudioInfo.hpp"
#include "solo.h"
#include "CoreFoundation/CoreFoundation.h"

#include <iostream>
#include <fstream>


#define __MALLOC   malloc
#define __FREE     free
#define __ASSERT   assert

const char* SilkEncoder::name = "silk";

vl_uint8 SilkEncoder::bpSample[1] = {16};

#define MAX_FRAME_PER_PACKET  (5)

vl_status SilkEncoder::updateParam() {
    if(0 == pthread_mutex_trylock(&stateMutex))
    {
        //安卓是没有下面这些的
        /*
        encControl.API_sampleRate = getSampleRate();
        encControl.bitRate = getBitrate();
        encControl.packetLossPercentage = SILK_ENC_CTL_PACKET_LOSS_PCT;
        encControl.complexity = 2;
        encControl.maxInternalSampleRate = encControl.API_sampleRate;
        */
        
        pthread_mutex_unlock(&stateMutex);
//        printf()<<"samplerate = "<<getSampleRate()<<getBitrate();
        return VL_SUCCESS;
    } else {
        return VL_FAILED;
    }
}


SilkEncoder::SilkEncoder(const AudioEncoderParam& encParam) :
    AudioEncoder(AUDIO_FORMAT_SILK, encParam), stEnc(NULL)
{
    enc_Ctrl.mode = 2;
    enc_Ctrl.targetRate_bps = 13600;
    enc_Ctrl.samplerate = 16000;
    enc_Ctrl.dtx_enable = 0;
    enc_Ctrl.joint_enable = 0;
    enc_Ctrl.joint_mode = 0;
    enc_Ctrl.framesize_ms = 40;
    enc_Ctrl.useMDIndex = 0;
    //zjw add
    stEnc = AGR_Sate_Encoder_Init(&enc_Ctrl);

    printf("SilkEncoder::SilkEncoder=========\n");
    updateParam();
    enabled = VL_TRUE;
}

SilkEncoder::~SilkEncoder() {
    if(NULL != stEnc) {
        __FREE(stEnc);
        stEnc = NULL;
    }
}

vl_bool SilkEncoder::setDTX(vl_bool enable) {
    
    //安卓没有这步
    /*
    encControl.useDTX = (VL_TRUE == enable) ? 1 : 0;
    printf("set dtx %d", encControl.useDTX);
    enable_DTX = enable;
    */
    return enable_DTX = 0;
}

vl_bool SilkEncoder::setFEC(vl_bool enable) {
    
    //安卓没有这步
    /*
    encControl.useInBandFEC = (VL_TRUE == enable) ? 1 : 0;
    printf("set fec %d", encControl.useInBandFEC);
    enable_FEC = enable;
     */
    return enable_FEC = 0;
}

vl_size SilkEncoder::getMaxPayloadSize() const {
    /*
   * assume bitrate of 40kpbs (5bytes / ms)
   */

    vl_size scpms = sampleRate / 1000;
    vl_size durMs = samplePerPackage / scpms;
    if(samplePerPackage % scpms != 0) {
        durMs ++;
    }
    vl_size frmPerRTP = durMs / getFrameMS();
    if(durMs % getFrameMS() != 0) {
        frmPerRTP ++;
    }
//    printf()<<"getMaxPayloadSize=========="<<scpms<<durMs<<frmPerRTP<<getFrameMS()<<(5 * durMs + AudPacket::getMaxBlockPaddingSize()) * frmPerRTP;
    return (5 * durMs + AudPacket::getMaxBlockPaddingSize()) * frmPerRTP;
}

vl_size SilkEncoder::getMaxPayloadSizePerFrame() const {
    /*
   * assume bitrate of 40kpbs (5bytes / ms)
   */
//    printf()<<"getMaxPayloadSizePerFrame()"<<getFrameMS();//5*20
    return 5 * getFrameMS();
}

vl_status SilkEncoder::getEncodedPacket(AudPacket* output)
{
    vl_status ret = VL_SUCCESS;
    vl_size sampleCount = samplePerPackage;//3200 原1600
    vl_uint16 frames;
    SKP_int result;
    vl_size samplePerFrame;
    SKP_int16* ptrSample = NULL;


    if(NULL == stEnc || VL_TRUE != enabled) {
        printf("silk encode error, encoder is not ready");
        return VL_ERR_CODEC_ENC_ERROR;
    }

    if(0 != pthread_mutex_trylock(&stateMutex) ) {
        printf("silk encode error, encoder is occupied");
        return VL_ERR_CODEC_ENC_ERROR;
    }

    if(sampleCount > circleBuffer->getLength() && VL_FALSE == circleBuffer->hasMoreData())  {
        sampleCount = circleBuffer->getLength();
    }

    /* specify sample count per audio packet */
    //指定每个音频数据包的采样数


    if(VL_SUCCESS != circleBuffer->getRange((vl_int16**)&ptrSample, sampleCount)) {
        return VL_ERR_CODEC_ENC_NOT_ENOUGH;
    }

    samplePerFrame = getSampleRate() * getFrameMS() / 1000;

    frames = sampleCount / samplePerFrame;

    if(0 != sampleCount % samplePerFrame) {
        frames ++;
        printf("silk encode ,input pcm duration is no match packge restrict");
    }
    int maxSamplePerPacket = 640;//编码块的大小 solo为640
    int encodedSamples = 0;
    //zjwadd
    unsigned char cbits[1920];
    short nBytes[6] = {0,0,0,0,0,0};
    char buffer[2000];//数据读取

    while(encodedSamples < sampleCount)
    {
        int leftSamples = sampleCount - encodedSamples;//剩余编码的长度
        int currEncSampleCount = (maxSamplePerPacket < leftSamples) ? maxSamplePerPacket : leftSamples;//已经要编码的长度 固定500还是最后剩余
        int currFrames = currEncSampleCount / samplePerFrame;//640/320=2
        if(currEncSampleCount % samplePerFrame != 0) {
            currFrames ++;
        }

        /* 从output中请求缓冲 */
        vl_int8* encPtr = NULL;
        int encBufferSize = getMaxPayloadSizePerFrame() * currFrames;//100*2
        
        SKP_int16 outSize;
        if(output->getLeftRange(&encPtr, encBufferSize) > 0)
        {
            
            outSize = encBufferSize;
            //zjw add
            memset(nBytes, 0, sizeof(short) * 6);
            result = AGR_Sate_Encoder_Encode(stEnc,ptrSample+encodedSamples,cbits, 1024, &nBytes[0]);
            memcpy(encPtr,nBytes,4);
            memcpy(encPtr+4,&cbits[0],result);
            
            if(0 != result) {
                output->fixBlockSize(result+4);
            } else {
                /* 编码失败 */
                printf("Silk encode failed, errno = %d", result);
                ret = VL_ERR_CODEC_ENC_ERROR;
                break;
            }
        } else {
            printf("silk encode, output packet has no enough buffer size=%d",encBufferSize);
            break;
        }

        /* 记录已编码的声音样本数 */
        encodedSamples += currEncSampleCount;
    }

    if(VL_FALSE == output->closePacket()) {
        printf("silk close output packet failed");
        ret = VL_ERR_CODEC_ENC_ERROR;
    }

    pthread_mutex_unlock(&stateMutex);
    return ret;

}

vl_status SilkEncoder::getEncFramePCMInfo(PCMInfo * info) const {
    if(VL_TRUE != this->enabled) {
        return VL_FAILED;
    }
    info->sample_cnt = getSampleRate() * getFrameMS()  / 1000;
    info->sample_rate = getSampleRate();
    info->channel_cnt = 1;
    info->bits_per_sample = getBytesPerSample() << 3;
//    printf()<<"SilkEncoder::getEncFramePCMInfo==="<<getSampleRate()<<getFrameMS()<<getBytesPerSample();
    return VL_SUCCESS;
}

SilkDecoder::SilkDecoder(const AudioDecoderParam& param) :
    AudioDecoder(AUDIO_FORMAT_SILK, param), stDec(NULL) {

//    printf("SilkDecoder::SilkDecoder==========\n");
    dec_Ctrl.packetLoss_perc = 0;
    dec_Ctrl.samplerate = 16000;
    dec_Ctrl.framesize_ms = 40;
    dec_Ctrl.joint_enable = 0;
    dec_Ctrl.joint_mode = 0;
    dec_Ctrl.useMDIndex = 0;
    dec_Ctrl.useMDIndex = 0;
    /* 初始化解码器 */
    stDec = AGR_Sate_Decoder_Init(&dec_Ctrl);

    if(stDec==NULL) {
        printf("silk initial decoder error\n");
        return;
    }
    updateParam();

    enabled = VL_TRUE;
}

SilkDecoder::~SilkDecoder() {
    if(NULL != stDec) {
        __FREE(stDec);
        stDec = NULL;
    }
}

vl_uint32 SilkDecoder::convertSampleRate(ECODEC_BAND_TYPE bandType) const {
    //   return ::silkConvertSampleRate(bandType);

    return PayloadTypeMapper::getSampleRateByBandtype(bandType);
}

vl_bool SilkDecoder::setPLC(vl_bool enable) {
    enable_plc = enable;
    return this->enable_plc;
}

vl_status SilkDecoder::updateParam() {
    if(0 == pthread_mutex_trylock(&stateMutex)) {
        pthread_mutex_unlock(&stateMutex);
        return VL_SUCCESS;
    } else {
        return VL_FAILED;
    }
}



vl_status SilkDecoder::decode(vl_int8* packet, vl_size size, UnitCircleBuffer* circleBuffer) {
    SKP_int result;

    do {
        //zjw
        vl_size samplePerFrame =getSampleRate() * getFrameMS() / 1000;//320
        SKP_int16* sampleOutPut = (SKP_int16*)circleBuffer->markRange(640);
        if(NULL == sampleOutPut) {
            printf("silk decode failed, not enough memory in circle buffer\n");
            result = -1;
            break;
        }
//        printf("pake size = %d\n",size);
        /*
        char buffer1[1500];
        strcpy(buffer1,getenv("HOME"));
        strcat(buffer1,"/Documents/output.dec");
        FILE *f = fopen(buffer1,"wb");
        if(f)
        {
            fwrite(packet,sizeof(signed char),size,f);
            fclose(f);
        }else{
            
            printf("文件名路径错误------------\n");
        }*/

        
        decode_file(packet,size,stDec,dec_Ctrl);
        
        /*
        char pcmbuffer[1500];
        strcpy(pcmbuffer,getenv("HOME"));
        strcat(pcmbuffer,"/Documents/out.pcm");
        
        FILE *pcm;
        pcm=fopen(pcmbuffer,"rb");
        
        if(pcm)
        {
            char buffer[1500];
            fread(buffer,1,640*2,pcm);
            memcpy(sampleOutPut,buffer,640*2);
            fclose(pcm);
        }else{
            printf("pcm文件名路径错误------------\n");
        }*/
        
        memcpy(sampleOutPut,pcmBuff,640*2);
        result = 0;
        if(SKP_SILK_NO_ERROR != result) {
            printf("SilkDecoder decode failed, errno = %d\n ", result);
            break;
        }

//        printf("SilkDecoder::decode---------------\n");
    } while( 0 );

    if(SKP_SILK_NO_ERROR != result) {
        return VL_ERR_CODEC_DEC_ERROR;
    } else {
        return VL_SUCCESS;
    }
}

/**
  * 解码pcm，需保证该函数可重入和线程安全。
  * 解码时，将一个包解析出来的帧数组逐个调用decode进行解码，也就是说，每次解码只会解码一帧数据
  * input : [in] 编码帧信息
  * output : [out] 解码出来的帧信息
  */
vl_status SilkDecoder::decode(AudPacket* input, UnitCircleBuffer* circleBuffer) {
    SKP_int result;
    SKP_int16 sampleCap;
    SKP_int16 sampleCount = 0;
    //   SKP_int16* sampleOutPut = (SKP_int16*) samples;
    vl_status ret;

    if(NULL == input || NULL == circleBuffer) {
        printf("silk decode error, input or sample is null");
        return VL_ERR_CODEC_DEC_ERROR;
    }

    if(NULL == this->stDec || VL_TRUE != enabled) {
        printf("silk decode error, decState is null or not enabled");
        return VL_ERR_CODEC_DEC_ERROR;
    }

    if(input->getFormat() != this->formatId) {
        printf("silk decode, audio format unmatched");
        return VL_ERR_CODEC_DEC_ERROR;
    }

    if(0 != pthread_mutex_trylock(&stateMutex) ) {
        printf("silk deocde error, decoder is occupied");
        return VL_ERR_CODEC_ENC_ERROR;
    }

    /* silk sample为16bit */
    //sampleCap = *length >> 1;

    vl_int8 * ptr;
    vl_size blockSize;

    if(VL_TRUE == input->isBlocked()) {
        while ((blockSize = input->getNextBlock(&ptr)) > 0)
        {
            ret = decode(ptr, blockSize, circleBuffer);
            if(ret != VL_SUCCESS) {
                printf("silk decode error : %d, size=%d", result, input->getCapacity());
                break;
            }
        }
    } else {
        ret = decode(input->getBuffer(), input->getCapacity(), circleBuffer);
    }

    pthread_mutex_unlock(&stateMutex);
    return ret;
}

vl_status SilkDecoder::getEncFramePCMInfo(PCMInfo *info) const {
    if(VL_TRUE != this->enabled) {
        return VL_FAILED;
    }
    //zjw
    info->sample_cnt = getSampleRate() * getFrameMS() / 1000/2;
    info->sample_rate = getSampleRate();
    info->channel_cnt = 1;
    info->bits_per_sample = 16;
    return VL_SUCCESS;
}
