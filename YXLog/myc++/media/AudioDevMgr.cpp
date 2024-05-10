
#include <stdio.h>

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <memory.h>

#include <malloc/malloc.h>

#include "StreamPlayScheduler.hpp"

#include "vl_const.h"
#include "AudioDevMgr.hpp"

#include "Time.hpp"
#include <printf.h>
#include "TestObject.hpp"


#define _MALLOC malloc
#define _FREE free
#define _MEMSET memset

//#define MIN_SIZE_PER_FRAME 5000   //每个包的大小,室内机要求为960,具体看下面的配置信息

#define SIZE_pbSize 1280   //传输字节大小 1280

#define musicSize 16000   //采集率  44000 16000

int  locale_afilePlay=0;

int  stop_pcm=0;//是否触发循环


AudioDevMgr* AudioDevMgr::instance = NULL;

static void generateNextFrame(vl_uint8* pcmData, vl_size length) {
    
//    printf("【超限制】generateNextFrame  %d\n ",length);

    memset(pcmData, 0, length);
}


/**
 AudioQueueRef                    inAQ    输出流
 AudioQueueBufferRef        inBuffer  输出字节大小
 */

void HandleOutputBuffer (
                         void                *auddev,
                         AudioQueueRef       inAQ,
                         AudioQueueBufferRef inBuffer
                         ) {
    
    if(auddev != NULL) {
        AudioDevMgr * pAuddev = (AudioDevMgr *) auddev;
        
        pAuddev->playerProc = VL_TRUE;
 /**
  2022.5.3
  在第一个缓冲区填充数据之前，iOS不会开始播放。如果此缓冲区很大，则可能需要1-2秒
  HandleOutputBuffer是当前流的接收，这里应该会耗费时间
  */
//        printf("缓冲区填充数据大小  %d\n ",inBuffer->mAudioDataByteSize);
        
        if(VL_TRUE == pAuddev->playing) {
            if(NULL != pAuddev->feeder) {
                /* 计算内部pcm缓冲可容纳多少个sample, 不考虑字节样本空缺 */
                pAuddev->feeder->feedPcm(pAuddev->getBlockNum, pAuddev->playerBuffer);
            }
            vl_int16* samples = NULL;
            
            /* 优先播放 */
            if(NULL != pAuddev->playerBuffer)
            {
                if(VL_SUCCESS != pAuddev->playerBuffer->getRange(&samples, pAuddev->playerBufferSize/2))
                {
                    if(pAuddev->playerBuffer->getLength() > 0 && VL_FALSE == pAuddev->playerBuffer->hasMoreData())
                    {
                        
                        /* 环形缓冲不会再有新数据输入，取出残留的数据 */
                        vl_size left = pAuddev->playerBuffer->getLength();
                        printf("AudioDevMgr player has no more data to feed, dump left %d \n", left);
                        pAuddev->playerBuffer->getRange(&samples, left);
                        memset(pAuddev->noiseBuffer, 0, pAuddev->playerBufferSize);
                        memcpy((void*)pAuddev->noiseBuffer, samples, left);
                        samples = (vl_int16*)pAuddev->noiseBuffer;
                        
                        
                    } else {
                        /* 无数据可写入播放缓冲，播放静音 */
                        printf("AudioDevMgr player has no enough data in circle buffer, expect %d, left %d !!!\n", pAuddev->playerBufferSize/2, pAuddev->playerBuffer->getLength());
                        generateNextFrame(pAuddev->noiseBuffer, pAuddev->playerBufferSize);
                        samples = (vl_int16*)pAuddev->noiseBuffer;
                        
                      
                    }
                }else {
                    stop_pcm=0;
                    memcpy((void*)pAuddev->noiseBuffer, samples, pAuddev->playerBufferSize);
                }
                
                /**
                 AudioQueueRef                    inAQ    输出流
                 AudioQueueBufferRef        inBuffer  输出字节大
                 */
                inBuffer->mAudioDataByteSize = pAuddev->playerBufferSize;
                
                bool isOpenSound = TestObject().palyIsOpenSound();

                if(!isOpenSound){
                    float gain = 0.0; // 设置增益值，范围为 0.0 到 1.0
    
                    int count = inBuffer->mAudioDataByteSize / sizeof(SInt16);
    
                    for (int i = 0; i < count; i++) {
                        samples[i] = samples[i] * gain;
                    }
                }else{
                    float gain = 1.0; // 设置增益值，范围为 0.0 到 1.0

                    int count = inBuffer->mAudioDataByteSize / sizeof(SInt16);

                    for (int i = 0; i < count; i++) {
                        samples[i] = samples[i] * gain;
                    }
                }
                
                
                memcpy(inBuffer->mAudioData, samples, pAuddev->playerBufferSize);
                
                
                
                AudioQueueEnqueueBuffer(pAuddev->mPlayQueue, inBuffer, 0,NULL);
//                printf("AudioQueueEnqueueBuffer把存有音频数据的Buffer插入到AudioQueue内置的Buffer队列中 :%d\n",pAuddev->playerBufferSize);
                //AudioQueueEnqueueBuffer把存有音频数据的Buffer插入到AudioQueue内置的Buffer队列中
                
            }
        }
        pAuddev->playerProc = VL_FALSE;
    }
    
    
}

OSStatus MyAudioUnitOutputBuffer(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                                    const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames,
                                 AudioBufferList *ioData) {
    
    if(inRefCon != NULL) {
        AudioDevMgr * pAuddev = (AudioDevMgr *) inRefCon;
        printf("AudioUnitRenderActionFlags %u , inBusNumber %d , inNumberFrames %d\n", *ioActionFlags, inBusNumber, inNumberFrames);
        pAuddev->playerProc = VL_TRUE;
        
        /**
         2022.5.3
         在第一个缓冲区填充数据之前，iOS不会开始播放。如果此缓冲区很大，则可能需要1-2秒
         HandleOutputBuffer是当前流的接收，这里应该会耗费时间
         */
       //        printf("缓冲区填充数据大小  %d\n ",inBuffer->mAudioDataByteSize);
        if(VL_TRUE == pAuddev->playing){
            
            if(NULL != pAuddev->feeder) {
                /* 计算内部pcm缓冲可容纳多少个sample, 不考虑字节样本空缺 */
                pAuddev->feeder->feedPcm(pAuddev->getBlockNum, pAuddev->playerBuffer);
            }
            vl_int16* samples = NULL;
            
            /* 优先播放 */
            if(NULL != pAuddev->playerBuffer)
            {
                if(VL_SUCCESS != pAuddev->playerBuffer->getRange(&samples, pAuddev->playerBufferSize/2))
                {
                    if(pAuddev->playerBuffer->getLength() > 0 && VL_FALSE == pAuddev->playerBuffer->hasMoreData())
                    {
                        
                        /* 环形缓冲不会再有新数据输入，取出残留的数据 */
                        vl_size left = pAuddev->playerBuffer->getLength();
                        printf("AudioDevMgr player has no more data to feed, dump left %d \n", left);
                        pAuddev->playerBuffer->getRange(&samples, left);
                        memset(pAuddev->noiseBuffer, 0, pAuddev->playerBufferSize);
                        memcpy((void*)pAuddev->noiseBuffer, samples, left);
                        samples = (vl_int16*)pAuddev->noiseBuffer;
                        
                        
                    } else {
                        /* 无数据可写入播放缓冲，播放静音 */
                        printf("AudioDevMgr player has no enough data in circle buffer, expect %d, left %d !!!\n", pAuddev->playerBufferSize/2, pAuddev->playerBuffer->getLength());
                        generateNextFrame(pAuddev->noiseBuffer, pAuddev->playerBufferSize);
                        samples = (vl_int16*)pAuddev->noiseBuffer;
                        
                        
                    }
                }else {
                    stop_pcm=0;
                    memcpy((void*)pAuddev->noiseBuffer, samples, pAuddev->playerBufferSize);
                }
                printf("将处理后的数据复制到输出缓冲区\n");
                /**
                 AudioQueueRef                    inAQ    输出流
                 AudioQueueBufferRef        inBuffer  输出字节大
                 */
                //                inBuffer->mAudioDataByteSize = pAuddev->playerBufferSize;
                
                bool isOpenSound = TestObject().palyIsOpenSound();
                
                if(!isOpenSound){
                    float gain = 0.0; // 设置增益值，范围为 0.0 到 1.0
                    
                    int count = pAuddev->playerBufferSize / sizeof(SInt16);
                    
                    for (int i = 0; i < count; i++) {
                        samples[i] = samples[i] * gain;
                    }

                }else{
                    float gain = 1.0; // 设置增益值，范围为 0.0 到 1.0
                    
                    int count = pAuddev->playerBufferSize / sizeof(SInt16);
                    
                    for (int i = 0; i < count; i++) {
                        samples[i] = samples[i] * gain;
                    }
                }
                
                // 将处理后的数据复制到输出缓冲区
                for (int i = 0; i < ioData->mNumberBuffers; i++) {
                    memcpy(ioData->mBuffers[i].mData, samples, pAuddev->playerBufferSize);
                    ioData->mBuffers[i].mDataByteSize = pAuddev->playerBufferSize;
                }
                
                // 使用AudioUnitRender将数据传递给AudioUnit进行播放
                AudioUnitRender(pAuddev->audioUnitPlay, ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
                
            }
            
        }
        pAuddev->playerProc = VL_FALSE;
    }
    
    return noErr;
}



void  HandleInputBuffer (
                         void                                *auddev,
                         AudioQueueRef                       inAQ,
                         AudioQueueBufferRef                 inBuffer,
                         const AudioTimeStamp                *inStartTime,
                         UInt32                              inNumPackets,
                         const   AudioStreamBasicDescription *inPacketDesc
                         )
{
    if(auddev != NULL) {
        AudioDevMgr*pAuddev = (AudioDevMgr*)auddev;
        
        pAuddev->recordProc = VL_TRUE;
        
        if(VL_TRUE == pAuddev->recording)
        {
            if(NULL != pAuddev->consumer)
            {
                
                /* 计算内部pcm缓冲可容纳多少个sample, 不考虑字节样本空缺 */
                vl_size sample_size = pAuddev->recordBufferSize * 8 / pAuddev->recordParam.pcmInfo.bits_per_sample;
                vl_size durationMS = sample_size * 1000 / pAuddev->recordParam.pcmInfo.sample_rate;
                
                
                if(pAuddev->onRecordCallback(durationMS))
                {
                    vl_uint8 * output_ptr = (vl_uint8*)inBuffer->mAudioData;
                    pAuddev->consumer->consumePcm(&pAuddev->recordParam.pcmInfo, output_ptr, &sample_size);
                }
            }
            
            if(false == pAuddev->timeToStopRecord())
            {
                AudioQueueEnqueueBuffer(pAuddev->mRecordQueue, inBuffer, 0,NULL);
            } else {
                
                pAuddev->recordUpdateStopTS();
                if(VL_TRUE == pAuddev->recording) {
                    pAuddev->recording = VL_FALSE;
                    
                    do {
                        /* 停止录音 */

                           int result = AudioQueueStop(pAuddev->mRecordQueue, TRUE);
                            if (result != 0) {

                            printf("AudioDevMgr opensl stop record, set record state error : %d",
                                 result);
                            //      LOGW("audio record lock stopRecord -- ");
                            break;
                        }
                    } while (0);
                    
                    StreamPlayScheduler::getInstance()->wakeup();
                } else {
                    printf("AudioDevMgr opensl recorder is not recoding now\n");
                }
                
            }
        }
    }
    
}





OSStatus MyAudioUnitRecordingCallback(void *inRefCon,
                                      AudioUnitRenderActionFlags *ioActionFlags,
                                      const AudioTimeStamp *inTimeStamp,
                                      UInt32 inBusNumber,
                                      UInt32 inNumberFrames,
                                      AudioBufferList *bufferList) {
    printf("数据转换\n");
    AudioDevMgr *pAuddev = (AudioDevMgr *)inRefCon;
    
    AudioBufferList *iosBufferList = (AudioBufferList *)malloc(sizeof(AudioBufferList));
    iosBufferList->mNumberBuffers = 1; //声道数，如果是录制立体声，这里应该是2
    iosBufferList->mBuffers[0].mNumberChannels = 1;
    //下面是这个结构要缓存的音频的大小
    iosBufferList->mBuffers[0].mDataByteSize = inNumberFrames * sizeof(SInt16) * 2;
    iosBufferList->mBuffers[0].mData = malloc(iosBufferList->mBuffers[0].mDataByteSize);
    
    memset(iosBufferList->mBuffers[0].mData, 0, iosBufferList->mBuffers[0].mDataByteSize);
    
    pAuddev->recordProc = VL_TRUE;
    printf("回调开始\n");
    if (VL_TRUE == pAuddev->recording) {
        
        if (NULL != pAuddev->consumer) {
            
            printf("继续录制\n");
            // 调用AudioUnitRender将音频数据添加到录制的AudioUnit中
            OSStatus status = AudioUnitRender(pAuddev->audioUnit,
                                              ioActionFlags,
                                              inTimeStamp,
                                              inBusNumber,
                                              inNumberFrames,
                                              iosBufferList);

            if (status != noErr) {
                printf("RecordCallback AudioUnitRender error %d \n", status);
            }
            
            UInt32 bufferSize = iosBufferList->mBuffers[0].mDataByteSize;
            /* 计算内部pcm缓冲可容纳多少个sample, 不考虑字节样本空缺 */
            vl_size sample_size = bufferSize * 8 / pAuddev->recordParam.pcmInfo.bits_per_sample;
            vl_size durationMS = sample_size * 1000 / pAuddev->recordParam.pcmInfo.sample_rate;

            printf("输出计算的sample_size>>>%u, durationMS>>>>>>>%u bufferSize >>>>> %d \n", sample_size, durationMS, bufferSize);
            printf("检查mNumberBuffers是否正确 >>>>%d \n", iosBufferList->mNumberBuffers);
            
            UInt32 NewbufferSize = bufferSize / 2;
            if (pAuddev->onRecordCallback(durationMS)) {
//                printf("打印其内容：%s", bufferList->mBuffers[0].mData);
                
                vl_uint8 *output_ptr = (vl_uint8 *)iosBufferList->mBuffers[0].mData;
               
                
                OSStatus consumePcmstatus = pAuddev->consumer->consumePcm(&pAuddev->recordParam.pcmInfo, output_ptr, &NewbufferSize);//error
                
                if(consumePcmstatus != noErr){
                    printf("pAuddev->consumer->consumePcm error \n");
                }else{
                    printf("consumePcm >>>sucess \n");
                }
                
            }
        }
            
        if (false == pAuddev->timeToStopRecord()) {
            // 继续录制
            // 获取音频数据的指针和大小
            printf("继续录制\n");
            
        } else {
            // 停止录制
            printf("停止录制\n");
//            pAuddev->recordUpdateStopTS();
//            if (VL_TRUE == pAuddev->recording) {
//                pAuddev->recording = VL_FALSE;
//                
//                OSStatus stopRes = AudioOutputUnitStop(pAuddev->audioUnit);
//                if (stopRes != noErr) {
//                    printf("---【播放器】----- 结束错误-----\n");
//                }
//                
//                if (stopRes ==0) {
//                    // 在释放 bufferList 之前，先对 audioUnit 进行未初始化操作
//                    AudioUnitUninitialize(pAuddev->audioUnit);
//                    
//                    // 释放 AudioUnit 相关资源
//                    AudioComponentInstanceDispose(pAuddev->audioUnit);
//                    pAuddev->audioUnit = NULL;
//                    
//    
//                }else{
//                    printf("    ---【播放器】----- 结束错误-----");
//                    
//                    AudioUnitUninitialize(pAuddev->audioUnit);
//                    
//                    // 释放 AudioUnit 相关资源
//                    AudioComponentInstanceDispose(pAuddev->audioUnit);
//                    pAuddev->audioUnit = NULL;
//                }
//                
//                StreamPlayScheduler::getInstance()->wakeup();
//            } else {
//                printf("AudioDevMgr opensl recorder is not recording now\n");
//            }
        }
    }
    
    free(iosBufferList->mBuffers[0].mData);
    free(iosBufferList);
    printf("noErr\n");
    
    return noErr;
}



void AudioDevMgr::reset()  {
    isReady = VL_FALSE;
    playing = VL_FALSE;
    playerId = AUDDEV_INVALID_ID;
    playerOccupied = VL_FALSE;
    
    recording = VL_FALSE;
    recordId = AUDDEV_INVALID_ID;
    recordOccupied = VL_FALSE;
    consumer = NULL;
    feeder = NULL;
    
    recordWouldDropMS = 0;
    recordMarkSavedMS = 0;
    
    isLongTimeOpenRecorder = false;
    }

void AudioDevMgr::initial() {
    do {
        
        pthread_mutex_init(&playerLock, NULL);
        pthread_mutex_init(&recordLock, NULL);
        
        /* 初始化成功 */
        isReady = VL_TRUE;
        printf("recordLock初始化成功\n");
//        printf("AudioDevMgr Audio device initial done==========\n\n");
        return;
    } while(0);
    
    /* 初始化失败 */
    printf("recordLock初始化失败\n");
    deinitial();
}

void AudioDevMgr::deinitial() {
    isReady = VL_FALSE;
    
    pthread_mutex_destroy(&playerLock);
    pthread_mutex_destroy(&recordLock);
    
    printf("AudioDevMgr Audio device deinitial done\n\n");
}

AudioDevMgr::AudioDevMgr() {
    /* 重置所有成员为未初始化状态 */
//    printf("1111111111111audiodevmgr---------\n\n");
    reset();
    /* 初始化OpenSL engine */
    initial();
}

AudioDevMgr::~AudioDevMgr() {
    /* 释放播放设备 */
    releasePlayer(playerId);
    /* 试图释放录音设备 */
    releaseRecorder(recordId);
    /* 释放OpenSL engine */
    deinitial();
}

AudioDevMgr* AudioDevMgr::getInstance()
{
    if(instance == NULL) {
        instance = new AudioDevMgr();
    }
    return instance;
}

bool AudioDevMgr::initialPlayer(const PlayParameter& param) {
    vl_status status = VL_SUCCESS;
//    printf("AudioDevMgr Audio device initial done==\n\n");
    if(VL_SUCCESS != status) {
        return false;
    }
    
    playerParam = param;
    //    playerParam.streamType=1;
    return true;
}

vl_status AudioDevMgr::aquirePlayer(const PlayParameter& param, PCMFeeder* feeder, int* handler, UnitCircleBuffer* circleBuf) {
    
    vl_status status = VL_SUCCESS;
    
    
    *handler = AUDDEV_INVALID_ID;
    
    if(VL_FALSE == isReady) {
        return VL_ERR_AUDDEV_INITAL;
    }
    
//    printf("AudioDevMgr opensl player param sr=%d, cc=%d, sc=%d, bps=%d\n",
//           param.pcmInfo.sample_rate, param.pcmInfo.channel_cnt, param.pcmInfo.sample_cnt, param.pcmInfo.bits_per_sample);
    
    
    
    *handler = getPlayer();
    if(AUDDEV_INVALID_ID == *handler) {
        printf("AudioDevMgr aquire player failed, device occupied !!!\n");
        return VL_ERR_AUDDEV_REC_OCCUPIED;
    }
    
    /* 复制参数 */
    this->playerParam = param;
    this->feeder = feeder;
    
    
    //audioUnit
//    playDesc.componentType = kAudioUnitType_Output;
//    playDesc.componentSubType = kAudioUnitSubType_RemoteIO; //kAudioUnitSubType_VoiceProcessingIO
//    playDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
//    playDesc.componentFlags = 0;
//    playDesc.componentFlagsMask = 0;
//
//    playComponent = AudioComponentFindNext(NULL, &playDesc);
//
//    if (playComponent != NULL) {
//        printf("playComponentFindNext 成功\n");
//    } else {
//        printf("playComponentFindNext 失败\n");
//        // 添加适当的错误处理逻辑
//        status = VL_ERR_AUDDEV_PLY_INIT;
//    }
//    
//    OSStatus instanceStatus = AudioComponentInstanceNew(playComponent, &audioUnitPlay);
//    if (instanceStatus == noErr) {
//        printf("playComponentInstanceNew 成功\n");
//    } else {
//        printf("playComponentInstanceNew 失败，错误码: %d\n", instanceStatus);
//        // 添加适当的错误处理逻辑
//        status = VL_ERR_AUDDEV_PLY_INIT;
//    }
//    
//    memset(&playRecordDataFormat, 0, sizeof(playRecordDataFormat));
//    playRecordDataFormat.mSampleRate = musicSize;
//    playRecordDataFormat.mFormatID = kAudioFormatLinearPCM;
//    playRecordDataFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked | kAudioFormatFlagsCanonical;
//    playRecordDataFormat.mBytesPerPacket = 2;
//    playRecordDataFormat.mFramesPerPacket = 1;
//    playRecordDataFormat.mBytesPerFrame = 2;
//    playRecordDataFormat.mChannelsPerFrame = 1;
//    playRecordDataFormat.mBitsPerChannel = 16;
//
//    OSStatus UnitSetstatus = AudioUnitSetProperty(audioUnitPlay, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &playRecordDataFormat, sizeof(playRecordDataFormat));
//    if (UnitSetstatus == noErr) {
//        printf("设置recordDataFormat属性成功\n");
//    } else {
//        printf("设置recordDataFormat属性失败，错误码: %d\n", UnitSetstatus);
//        // 添加适当的错误处理逻辑
//        status = VL_ERR_AUDDEV_PLY_INIT;
//    }
//    //AEC
//    UInt32 echoCancellation = kAUVoiceIOSpeechActivityHasStarted;
//    UInt32 echoCancellationSize = sizeof(echoCancellation);
//
//    AudioUnitSetProperty(audioUnitPlay,
//                         kAUVoiceIOProperty_BypassVoiceProcessing,
//                         kAudioUnitScope_Output,
//                         0,
//                         &echoCancellation,
//                         echoCancellationSize);
//    // AGC
//    UInt32 echoCancellationMode = kAUVoiceIOSpeechActivityHasStarted;
//    AudioUnitSetProperty(audioUnitPlay,
//                         kAUVoiceIOProperty_VoiceProcessingEnableAGC,
//                         kAudioUnitScope_Output,
//                         0,
//                         &echoCancellationMode,
//                         sizeof(echoCancellationMode));
//
////        // NS
//    UInt32 noiseSuppressionQuality = kAUVoiceIOSpeechActivityHasStarted;
//    AudioUnitSetProperty(audioUnitPlay,
//                         kAUVoiceIOProperty_MuteOutput,
//                         kAudioUnitScope_Output,
//                         0,
//                         &noiseSuppressionQuality,
//                         sizeof(noiseSuppressionQuality));
//    
//    recordPlayback.inputProc = MyAudioUnitOutputBuffer;
//    recordPlayback.inputProcRefCon = (void * _Nullable)this;
//
//    OSStatus InputCallbackstatus = AudioUnitSetProperty(audioUnitPlay, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &recordPlayback, sizeof(recordPlayback));
//    if (InputCallbackstatus == noErr) {
//        printf("设置回调成功\n");
//    } else {
//        printf("设置回调失败，错误码: %d\n", InputCallbackstatus);
//        // 添加适当的错误处理逻辑
//        status = VL_ERR_AUDDEV_PLY_INIT;
//    }
//    
//    //开启audiounit
//    OSStatus Initialstatus = AudioUnitInitialize(audioUnitPlay);
//    if (Initialstatus == noErr) {
//        printf("Audio unit initialized successfully \n");
//    } else {
//        printf("Audio unit initialization failed: %d \n", Initialstatus);
//        // 添加适当的错误处理逻辑
//        status = VL_ERR_AUDDEV_PLY_INIT;
//    }
    
    
    mPlayDataFormat.mFormatID           = kAudioFormatLinearPCM;
    mPlayDataFormat.mSampleRate         = musicSize;//采集率8k～16k
    mPlayDataFormat.mChannelsPerFrame   =  1;//声道数，编解码暂时只支持1
    mPlayDataFormat.mBitsPerChannel     = 16;//每个sample占用比特数，编解码暂时只支持16（两个字节）
    mPlayDataFormat.mFramesPerPacket    = 1;
    mPlayDataFormat.mFormatFlags        =  kLinearPCMFormatFlagIsSignedInteger|kLinearPCMFormatFlagIsPacked|kAudioFormatFlagsCanonical;//是对应音频格式的ID，即各种格式；每种格式在API有对应说明，如kAudioFormatLinearPCM等
    
//    mPlayDataFormat.mFormatFlags      =kAudioFormatFlagsCanonical;//是对应音频格式的ID，即各种格式；每种格式在API有对应说明，如kAudioFormatLinearPCM等

    mPlayDataFormat.mBytesPerFrame      = 2;//每个采样点16bit量化 语音每采样点占用位数 (mPlayDataFormat.mBitsPerChannel / 8) * mPlayDataFormat.mChannelsPerFrame
    mPlayDataFormat.mBytesPerPacket     = 2;//每个数据包的bytes总数 mPlayDataFormat.mBytesPerFrame * mPlayDataFormat.mFramesPerPacket
    
    
    // 使用player的内部线程播放 新建输出
    OSStatus output_status = AudioQueueNewOutput(&mPlayDataFormat,
                                                 (AudioQueueOutputCallback)HandleOutputBuffer,
                                                 (void *)this,
                                                 NULL,
                                                 NULL,
                                                 0,
                                                 &mPlayQueue);
    
    
    //    printf("-------------------");
    //    printf("\n采样率:%.f\n通道数:%u\n采样位数:%u\n",mPlayDataFormat.mSampleRate,(unsigned int)mPlayDataFormat.mChannelsPerFrame,(unsigned int)mPlayDataFormat.mBitsPerChannel);
    

    // 设置音量
//    AudioQueueSetParameter(mPlayQueue, kAudioQueueParam_Volume, 1.0);
//    printf("---------output_status-------------%d- \n",(int)output_status);
    
    if(output_status != 0) {
        status = VL_ERR_AUDDEV_PLY_INIT;
//        printf("---------VL_ERR_AUDDEV_PLY_INIT--------------\n\n");
    }
    
    
    /* 初始化外部播放缓冲
      采样率，如 8000, 16000 ...
     vl_uint32 sample_rate;
      表示语音时长
     vl_size  sample_cnt;
      声道数，编解码暂时只支持1
     vl_uint32 channel_cnt;
      每个sample占用比特数，编解码暂时只支持16（两个字节），后续需要更改则需要resample
     vl_uint32 bits_per_sample;
     */
    playerBufferSize= playerParam.pcmInfo.sample_cnt* playerParam.pcmInfo.channel_cnt*playerParam.pcmInfo.bits_per_sample/8;
//    printf("播放器player播放字节长度: %d -- 表示语音时长:%d\n",playerBufferSize,playerParam.pcmInfo.sample_cnt);
    playerBufferSize=SIZE_pbSize;
    
    char buffer[256];
    strcpy(buffer,getenv("HOME"));
    strcat(buffer,"/Documents/EnterBackground.txt");
    char str[100];//缓冲区，用来储存数据
    FILE *f = fopen(buffer,"rb");
    if(f){ fgets(str,100,f); }
    int num1=atoi(str);
    
    if (num1>0) {
        playerBufferSize=SIZE_pbSize;
    }
    
    
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        AudioQueueAllocateBuffer(mPlayQueue, playerBufferSize, &mPlayBuffers[i]);
        mPlayBuffers[i]->mAudioDataByteSize = playerBufferSize;
//        printf("【**】数据大小： %d\n",playerBufferSize);
        AudioQueueEnqueueBuffer(mPlayQueue, mPlayBuffers[i], 0,NULL);
    }
    
    
    playerBuffer = circleBuf;
    
    /**
     缓冲大小是播放字节长度 *    1000 /2。这里不知道干嘛用的
     */
//    new UnitCircleBuffer(playerBufferSize * PLAY_NUM_BUFFERS/2);
    
    
    /* 若circlebuf没有初始化大小，初始化缓冲 */
    if(circleBuf->getCapacity() == 0) {
//        printf("若circlebuf没有初始化大小，初始化缓冲\n");
        circleBuf->init(playerBufferSize * PLAY_NUM_BUFFERS);
    }
    
//    std::unique_ptr<vl_uint8[]> noiseBuffer;  // 使用std::unique_ptr作为智能指针
//    noiseBuffer = std::make_unique<vl_uint8 *>(playerBufferSize * 100);
    
    noiseBuffer = (vl_uint8*)_MALLOC(playerBufferSize*100);
    
    /* 暂时处理为静音 */
    memset(noiseBuffer, 0, playerBufferSize);
    
//    printf("AudioDevMgr aquire player id = %d\n", playerId);
    //    recordOccupied = VL_TRUE;
    printf("AudioDevMgr Audio device aquire player done !!!\n\n");
    
    return status;
}



vl_status AudioDevMgr::reconfigPlayer(const PlayParameter& param, int handler, UnitCircleBuffer* circleBuf) {
    
    
    printf("AudioDevMgr opensl reconfig player stop play failed\n");

    vl_status status;
    if(handler != playerId) {
        return VL_ERR_AUDDEV_ID;
    }
    
    PCMInfo& oldInfo = this->playerParam.pcmInfo;
    const PCMInfo& newInfo = param.pcmInfo;
    
    if(oldInfo == newInfo) {
        return VL_TRUE;
    }
    
    if(VL_TRUE == playing) {
        /* 当前正在播放 */
        do {
            status = stopPlay(handler, VL_FALSE);
            if(VL_SUCCESS != status) {
                printf("AudioDevMgr opensl reconfig player stop play failed\n");
                break;
            }
            
            PCMFeeder* oldFeader = feeder;
            int tempHandler;
            
            status = releasePlayer(handler);
            if(VL_SUCCESS != status){
                printf("AudioDevMgr opensl reconfig player release player failed\n");
                break;
            }
            
            status = aquirePlayer(param, oldFeader, &tempHandler, circleBuf);
            if(VL_SUCCESS != status) {
                printf("AudioDevMgr opensl reconfig player aquire failed\n");
                break;
            }
            playerId = handler;
            
            status = startPlay(handler);
            if(status != VL_SUCCESS) {
                printf("AudioDevMgr opensl reconfig player play failed\n");
                break;
            }
        } while(0);
    } else {
        do {
            PCMFeeder* oldFeader = feeder;
            int tempHandler;
            
            status = releasePlayer(handler);
            if(VL_SUCCESS != status){
                printf("AudioDevMgr opensl reconfig player release player failed\n");
                break;
            }
            
            status = aquirePlayer(param, oldFeader, &tempHandler, circleBuf);
            if(VL_SUCCESS != status) {
                printf("AudioDevMgr opensl reconfig player aquire failed\n");
                break;
            }
            playerId = handler;
            
        } while(0);
    }
    
    return status;
}



vl_status AudioDevMgr::incVolume() {
    return 10;
    
}

vl_status AudioDevMgr::decVolume() {
    return 10;
    
}

vl_status AudioDevMgr::setVolume(vl_int16 volume) {
    return 32;
    
}

vl_status AudioDevMgr::pausePlay(int handler) {
    OSStatus result;
    if(handler != playerId) {
        return VL_ERR_AUDDEV_ID;
    }
    if(mPlayQueue == NULL) {
        printf("AudioDevMgr opensl play_pause failed, playerPlay is null\n");
        return VL_ERR_AUDDEV_PLY_INIT;
    }
    
    
    AudioQueueFlush(mPlayQueue);
    result = AudioQueueStop(mPlayQueue, TRUE);
    
//    if(audioUnitPlay == NULL) {
//        printf("AudioDevMgr opensl play_pause failed, playerPlay is null\n");
//        return VL_ERR_AUDDEV_PLY_INIT;
//    }
//    // 停止AudioUnit
//    result = AudioOutputUnitStop(audioUnitPlay);
//
//    // 重置AudioUnit
//    AudioUnitReset(audioUnitPlay, kAudioUnitScope_Output, 0);
    
//    printf("-----------【播放器】-----pausePlay 停止   \n");

    if (result != 0) {
        
        printf("AudioDevMgr opensl play_pause error : %d\n", result);
        return VL_ERR_AUDDEV_PLY_OP;
    }
    
    return VL_SUCCESS;
}

    
vl_status AudioDevMgr::resumePlay(int handler) {
    OSStatus result;
    if(handler != playerId) {
        return VL_ERR_AUDDEV_ID;
    }
    if(mPlayQueue == NULL) {
        printf("AudioDevMgr opensl play_resume failed, playerPlay or playBufQ is null\n");
        return VL_ERR_AUDDEV_PLY_NOT_READY;
    }
    result = AudioQueueStart(mPlayQueue, NULL);
    
//    if(audioUnitPlay == NULL) {
//        printf("AudioDevMgr opensl play_resume failed, playerPlay or playBufQ is null\n");
//        return VL_ERR_AUDDEV_PLY_NOT_READY;
//    }
//    result = AudioOutputUnitStart(audioUnitPlay);
    
    if (result != 0) {
        printf("AudioDevMgr opensl play_reume failed : %d\n", result);
        return VL_ERR_AUDDEV_PLY_OP;
    }
    return VL_SUCCESS;
}
#include "conference.h"
    
vl_status AudioDevMgr::startPlay(int handler) {
    
//    printf("locale_afilePlay的状态是>>>>>%d\n\n", locale_afilePlay);
    
    if (locale_afilePlay!=1) {
//            usleep(500*1000);
        printf("播放前的数据显示>>>>>>>>>%s\n\n", recvBuff);
        // TODO: 叶炽华 联调
//        TestObject().testFunction(recvBuff);
        
//            time_t starttime;
//            time(&starttime);
        
//            TestObject().playFunction();
    }
    
    printf("-----------------开始播放---------------------\n\n");
    usleep(800*1000);
    time_t starttime;
    time(&starttime);
    
    printf("1【接收---开始播放】当前时间戳：%ld\n",starttime);
    
    
    vl_status status = VL_ERR_AUDDEV_PLY_OP;
    OSStatus result;
    if( mPlayQueue==NULL || feeder == NULL) {
        printf("AudioDevMgr opensl play_start failed, playerPlay or playerBufq is null\n\n");
        return VL_ERR_AUDDEV_PLY_INIT;
    }
//    if( audioUnitPlay==NULL || feeder == NULL) {
//        printf("AudioDevMgr opensl play_start failed, playerPlay or playerBufq is null\n\n");
//        return VL_ERR_AUDDEV_PLY_INIT;
//    }
    /* 单例模式，防止未调用aquire而调用播放请求，需持有当前播放id */
    if(handler != playerId) {
        printf("未调用aquire而调用播放请求，需持有当前播放id\n\n");
        return VL_ERR_AUDDEV_ID;
    }
    pthread_mutex_lock(&playerLock);
    
    /* 当前已在播放状态 */
    if(VL_TRUE == playing) {
        
        printf("AudioDevMgr opensl start play: already playing\n\n");
        // TODO: 叶炽华 联调
//        TestObject().playFunction();
        status = VL_SUCCESS;
    } else {
        do {
            playing = VL_TRUE;
            result = AudioQueueStart(mPlayQueue, NULL);
//            result = AudioOutputUnitStart(audioUnitPlay);
            if (result != 0) {
                printf("AudioDevMgr opensl play start, set play state error : %d\n", result);
                break;
            }
            
            playerProc = VL_FALSE;
            // TODO: 叶炽华 联调
//            TestObject().playFunction();
            status = VL_SUCCESS;
        } while(0);
    }
    pthread_mutex_unlock(&playerLock);
    printf("最后返回的状态: %d\n", status);
    
    return status;
}

#include "conference.h"
vl_status AudioDevMgr::stopPlay(int handler, vl_bool wait) {
    vl_status status = VL_ERR_AUDDEV_PLY_OP;
    getBlockNum = 0;
  
    time_t stoptime;
    time(&stoptime);
    char buff[100];
    sprintf(buff, "%d,%ld",locale_afilePlay,stoptime);
//    printf("【接收---结束】当前时间戳：%ld\n",stoptime);
    printf("关闭本地文件  localefilePlay : %d\n", locale_afilePlay);
    


//    if (locale_afilePlay==0) {
//        TestObject().stopFunction(buff);
//    }
    
    // TODO: 叶炽华 联调
//    TestObject().stopFunction(buff);
//    locale_afilePlay=0;
//    memset(recvBuff, 0, sizeof(recvBuff));
    

    
    
    /* 单例模式，防止未调用aquire而调用播放请求，需持有当前播放id */
    if(handler != playerId) {
        printf("AudioDevMgr %s stop play not valid handler \n", __FILE__);
        return VL_ERR_AUDDEV_ID;
    }
    
    OSStatus result;
    if(mPlayQueue == NULL)
    {
        return VL_ERR_AUDDEV_PLY_INIT;
    }
//    if(audioUnitPlay == NULL)
//    {
//        return VL_ERR_AUDDEV_PLY_INIT;
//    }
    pthread_mutex_lock(&playerLock);
    
    if(VL_FALSE != playing) {
        playing = VL_FALSE;
        
        do {
            
            AudioQueueFlush(mPlayQueue);
            result = AudioQueueStop(mPlayQueue, TRUE);
            
//            result = AudioOutputUnitStop(audioUnitPlay);
//
//                // 重置AudioUnit
//            AudioUnitReset(audioUnitPlay, kAudioUnitScope_Output, 0);
            
            if (result != 0)
            {
                printf("AudioDevMgr opensl play_stop failed : %d\n", result);
                break;
            }
            printf("-----------【播放器】-----stopPlay停止 \n");
                        
            status = VL_SUCCESS;
        } while (0);
    }
    
    free(noiseBuffer);
    noiseBuffer = NULL;
    
    pthread_mutex_unlock(&playerLock);
    return status;
}



vl_status AudioDevMgr::releasePlayer(int handler) {
    if(handler != playerId) {
        printf("AudioDevMgr release player not valid id\n");
        return VL_ERR_AUDDEV_ID;
    }
    
    AudioQueueFlush(mPlayQueue);
    AudioQueueStop(mPlayQueue, TRUE);
    
//    AudioOutputUnitStop(audioUnitPlay);
//
//    // 重置AudioUnit
//    AudioUnitReset(audioUnitPlay, kAudioUnitScope_Output, 0);
    
    printf("-----------【播放器】-----releasePlayer 停止 \n");

    for (int i = 0; i < NUM_BUFFERS; i++) {
        AudioQueueFreeBuffer(mPlayQueue, mPlayBuffers[i]);
    }
//    AudioComponentInstanceDispose(audioUnitPlay);
    
    time_t stoptime;
    time(&stoptime);
    char buff[100];
    sprintf(buff, "%d,%ld",locale_afilePlay,stoptime);
//    printf("【接收---结束】当前时间戳：%ld\n",stoptime);
//    printf("关闭本地文件  localefilePlay : %d\n", locale_afilePlay);
    
    TestObject().stopFunction(buff);
    playing = VL_FALSE;
    AudioQueueDispose(mPlayQueue, FALSE);
    mPlayQueue = NULL;
//    audioUnitPlay = NULL;
    
    putPlayer(handler);
//    free(noiseBuffer);
    printf("-----------销毁播放器-----------");
    return VL_SUCCESS;
}


bool AudioDevMgr::initialRecorder(const RecordParameter& param)
{
    printf(" AudioDevMgr::initialRecorder==============\n");
    //vl_status status = VL_ERR_AUDDEV_REC_INIT;
    return true;
}

vl_status AudioDevMgr::aquireRecorder(const RecordParameter& param, PCMConsumer* consumer, int* handler)
{
    
    printf("准备录音设备。。。。。。。。。。。。。。。。%d\n",*handler);
    vl_status status = VL_ERR_AUDDEV_REC_INIT;
    OSStatus result;
    *handler = AUDDEV_INVALID_ID;
    
    if(VL_FALSE == isReady) {
        printf("AudioDevMgr aquire recorder failed, device not ready !!!!\n");
        return VL_ERR_AUDDEV_INITAL;
    }
    
    printf("AudioDevMgr opensl recorder param sr=%d, cc=%d, sc=%d, bps=%d\n\n",
           param.pcmInfo.sample_rate, param.pcmInfo.channel_cnt, param.pcmInfo.sample_cnt, param.pcmInfo.bits_per_sample);
    
    *handler = getRecorder();
    if(AUDDEV_INVALID_ID == *handler) {
        printf("AudioDevMgr aquire recorder failed, device occupied !!!\n");
        return VL_ERR_AUDDEV_REC_OCCUPIED;
    }
    do{
        
//        //设置为播放和录音状态，以便可以在录制完之后播放录音
//        [audioSession setCategory:AVAudioSessionCategoryPlayAndRecord error:nil];
//        [audioSession setActive:YES error:nil];
//
//                memset(&mRecordDataFormat, 0, sizeof(mRecordDataFormat));
        
//        UInt32 size = sizeof(mRecordDataFormat.mSampleRate);
//        AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareSampleRate, &size, &mRecordDataFormat.mSampleRate);
//        size = sizeof(mRecordDataFormat.mChannelsPerFrame);
//        AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareInputNumberChannels, &size, &mRecordDataFormat.mChannelsPerFrame);

        
//        mRecordDataFormat.mFormatID           = kAudioFormatLinearPCM;//编码格式
//        mRecordDataFormat.mSampleRate         = musicSize; //采样率
//        mRecordDataFormat.mChannelsPerFrame   = 1; //每帧的声道数
//        mRecordDataFormat.mBitsPerChannel     = 16; //每个声道的采样深度
//        mRecordDataFormat.mBytesPerFrame      = 2; //每帧的Byte数
//        mRecordDataFormat.mBytesPerPacket     = 2; //每个Packet的帧数
//        mRecordDataFormat.mFramesPerPacket    = 1;//每个Packet的Bytes数
////        mRecordDataFormat.mFormatFlags        =
////        kLinearPCMFormatFlagIsSignedInteger|kLinearPCMFormatFlagIsPacked|kAudioFormatFlagsCanonical;
//        //kAudioFormatFlagsCanonical;//数据格式；（L/R，整形or浮点）
//        mRecordDataFormat.mReserved = 0;
//        mRecordDataFormat.mFormatFlags        =kLinearPCMFormatFlagIsSignedInteger|kLinearPCMFormatFlagIsPacked;
//
//        recordBufferSize = SIZE_pbSize;
//
//
//        // Set up audio queue   999999999999
////        result=AudioQueueNewInput(&mRecordDataFormat,(AudioQueueInputCallback)HandleInputBuffer,(void *)this,NULL,NULL, 0,&mRecordQueue);
////         改kCFRunLoopCommonModes  变更成  NULL
//        result=AudioQueueNewInput(&mRecordDataFormat,(AudioQueueInputCallback)HandleInputBuffer,(void *)this,NULL,kCFRunLoopCommonModes, 0,&mRecordQueue);
//
//        if(result != 0) {
//            printf("AudioDevMgr AudioQueueNewInput failed: %d\n", result);
//        }
//
////         Enqueue buffer
//        for (int i = 0; i < NUM_BUFFERS; i++)
//        {
//            AudioQueueAllocateBuffer(mRecordQueue, recordBufferSize, &mRecordBuffers[i]);
//            mRecordBuffers[i]->mAudioDataByteSize = recordBufferSize;
//            AudioQueueEnqueueBuffer(mRecordQueue, mRecordBuffers[i], 0,NULL);
//        }
        
        //audioUnit
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_RemoteIO; //kAudioUnitSubType_VoiceProcessingIO
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;
        desc.componentFlags = 0;
        desc.componentFlagsMask = 0;

        component = AudioComponentFindNext(NULL, &desc);

        if (component != NULL) {
            printf("AudioComponentFindNext 成功\n");
        } else {
            printf("AudioComponentFindNext 失败\n");
            // 添加适当的错误处理逻辑
        }
        
        OSStatus instanceStatus = AudioComponentInstanceNew(component, &audioUnit);
        if (instanceStatus == noErr) {
            printf("AudioComponentInstanceNew 成功\n");
        } else {
            printf("AudioComponentInstanceNew 失败，错误码: %d\n", instanceStatus);
            // 添加适当的错误处理逻辑
        }
        
        
        memset(&recordDataFormat, 0, sizeof(recordDataFormat));
        recordDataFormat.mSampleRate = musicSize;
        recordDataFormat.mFormatID = kAudioFormatLinearPCM;
        recordDataFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked | kAudioFormatFlagsCanonical;
        recordDataFormat.mBytesPerPacket = 2;
        recordDataFormat.mFramesPerPacket = 1;
        recordDataFormat.mBytesPerFrame = 2;
        recordDataFormat.mChannelsPerFrame = 1;
        recordDataFormat.mBitsPerChannel = 16;

        
        
        OSStatus UnitSetstatus = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &recordDataFormat, sizeof(recordDataFormat));
        if (UnitSetstatus == noErr) {
            printf("设置recordDataFormat属性成功\n");
        } else {
            printf("设置recordDataFormat属性失败，错误码: %d\n", UnitSetstatus);
            // 添加适当的错误处理逻辑
        }
        
       
        
        //AEC
        UInt32 echoCancellation = kAUVoiceIOSpeechActivityHasStarted;
        UInt32 echoCancellationSize = sizeof(echoCancellation);

        AudioUnitSetProperty(audioUnit,
                             kAUVoiceIOProperty_BypassVoiceProcessing,
                             kAudioUnitScope_Global,
                             1,
                             &echoCancellation,
                             echoCancellationSize);
        // AGC
        UInt32 echoCancellationMode = kAUVoiceIOSpeechActivityHasStarted;
        AudioUnitSetProperty(audioUnit,
                             kAUVoiceIOProperty_VoiceProcessingEnableAGC,
                             kAudioUnitScope_Global,
                             1,
                             &echoCancellationMode,
                             sizeof(echoCancellationMode));

//        // NS
        UInt32 noiseSuppressionQuality = kAUVoiceIOSpeechActivityHasStarted;
        AudioUnitSetProperty(audioUnit,
                             kAUVoiceIOProperty_MuteOutput,
                             kAudioUnitScope_Global,
                             1,
                             &noiseSuppressionQuality,
                             sizeof(noiseSuppressionQuality));
        
        
        // 打开录制功能
        AudioUnitElement inputBus = 1; // 输入总线编号
        UInt32 enableInput = 1;
        OSStatus inputBusstatus = AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, inputBus, &enableInput, sizeof(enableInput));
        if (inputBusstatus == noErr) {
            printf("打开录制功能成功\n");
        } else {
            printf("打开录制功能失败，错误码: %d\n", inputBusstatus);
            // 添加适当的错误处理逻辑
        }
        
        
        recordCallback.inputProc = MyAudioUnitRecordingCallback;
        recordCallback.inputProcRefCon = (void * _Nullable)this;

        OSStatus InputCallbackstatus = AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Output, 1, &recordCallback, sizeof(recordCallback));
        if (InputCallbackstatus == noErr) {
            printf("设置回调成功\n");
        } else {
            printf("设置回调失败，错误码: %d\n", InputCallbackstatus);
            // 添加适当的错误处理逻辑
        }
        
        //开启audiounit
        OSStatus Initialstatus = AudioUnitInitialize(audioUnit);
        if (Initialstatus == noErr) {
            printf("Audio unit initialized successfully \n");
        } else {
            printf("Audio unit initialization failed: %d \n", Initialstatus);
            // 添加适当的错误处理逻辑
        }
        
        printf("AudioDevMgr recorder aquired : id=%d\n", recordId);
        status = VL_SUCCESS;
    }while(0);
    if(VL_SUCCESS == status)
    {
        this->recordParam = param;
        this->consumer = consumer;
    } else {
        releaseRecorder(*handler);
        printf("AudioDevMgr aquire recorder failed, put handler back\n");
    }
    printf("准备录音结束··················· \n");
    return status;
    
}




vl_status AudioDevMgr::reconfigRecorder(const RecordParameter& param, int handler)
{
    printf("reconfigrecoder.................qqqqqqqqqqqq.\n");
    vl_status status;
    
    if(handler != recordId) {
        return VL_ERR_AUDDEV_ID;
    }
    
    PCMInfo& oldInfo = this->recordParam.pcmInfo;
    const PCMInfo& newInfo = param.pcmInfo;
    
    if(oldInfo == newInfo)
    {
        return VL_TRUE;
    } else {
        if(VL_TRUE == recording)
        {
            do {
                status = stopRecord(handler);
                if(VL_SUCCESS != status)
                {
                    printf("AudioDevMgr reconfig recorder, stop failed\n");
                    break;
                }
                
                PCMConsumer* oldConsumer = this->consumer;
                int tempHandler;
                
                status = releaseRecorder(handler);
                if(VL_SUCCESS != status)
                {
                    printf("AudioDevMgr reconfig recorder, release failed\n");
                    break;
                }
//                printf("11111111111111111111\n");
                status = aquireRecorder(param, oldConsumer, &tempHandler);
                if(VL_SUCCESS != status)
                {
                    printf("AudioDevMgr reconfig recorder, aquire failed\n");
                    break;
                }
                this->recordId = handler;
//                printf("2222222222222222222222\n");
                status = startRecord(handler);
                if(VL_SUCCESS != status)
                {
                    printf("AudioDevMgr reconfig recorder, start failed\n");
                    break;
                }
            } while(0);
        } else {
            do {
                PCMConsumer* oldConsumer = this->consumer;
                int tempHandler;
                
                status = releaseRecorder(handler);
                if(VL_SUCCESS != status)
                {
                    printf("AudioDevMgr reconfig recorder, release failed\n");
                    break;
                }
                printf("333333333333333333\n");
                status = aquireRecorder(param, oldConsumer, &tempHandler);
                if(VL_SUCCESS != status)
                {
                    printf("AudioDevMgr reconfig recorder, aquire failed\n");
                    break;
                }
                this->recordId = handler;
            } while(0);
        }
    }
    
    return status;
}

vl_status AudioDevMgr::startRecord(int handler)
{
    
//    //   4.27   开启播放器
//    UInt32 audioRouteOverride = kAudioSessionOverrideAudioRoute_Speaker;
//    AudioSessionSetProperty (
//                             kAudioSessionProperty_OverrideAudioRoute,
//                             sizeof (audioRouteOverride),
//                             &audioRouteOverride
//                             );
    
    printf("开始录音啦······················\n");
    printf("先检查recording的参数：>>>> %d\n", recording);
    
    if(VL_TRUE == recording){
        recording = VL_FALSE;
    }
    
    printf("AudioDevMgr audiodev start record ...\n\n");
    vl_status status = VL_ERR_AUDDEV_STAT_REC_FAIL;
    OSStatus result;
    
    if(handler != recordId) {
        return VL_ERR_AUDDEV_ID;
    }
    
//    if(mRecordQueue==NULL || consumer == NULL)
    if(audioUnit==NULL || consumer == NULL)
    {
        //mRecordQueue。音频指向----------- consumerpcm对象。出错
        printf("AudioDevMgr opensl play_start failed, playerPlay or playerBufq is null\n\n");
        return VL_ERR_AUDDEV_REC_NOT_READY;
    }
    
//    printf("[MUTEX] LOCK %s %p\n",__FUNCTION__,&recordLock);
    pthread_mutex_lock(&recordLock);
    
    /* 多线程调用，进入先判断是否重复调用 startRecord */
    if(VL_TRUE == recording)
    {
        printf("---------正在录音---------- \n");
        
        printf("AudioDevMgr opensl is already recording\n");
        status = VL_SUCCESS;
    } else
    {
        printf("---------没有录音---------- \n");
        recordMarkStart();
        
        
        do {
            recording = VL_TRUE;
            
            //mRecordQueue   音频队列对象指针
//            result = AudioQueueStart(mRecordQueue, NULL);
            result = AudioOutputUnitStart(audioUnit);
            printf("---------开始录音--音频队列对象指针-------\n");
            
//            if (result != 0)
            if (result == noErr)
            {
                printf("音频单元启动成功\n");
            }else{
                printf("开始出错 begin");
                printf("AudioDevMgr opensl set record state to start record failed : %d  ............\n\n", result);
                break;
            }


            recordProc = VL_FALSE;
            
            
            status = VL_SUCCESS;
            
        } while(0);
        
    }
    printf("【AudioDevMgr】[MUTEX] unLOCK %s %p\n",__FUNCTION__,&recordLock);
    
    pthread_mutex_unlock(&recordLock);
//    pthread_mutex_unlock(&recordLock);
    printf("走完 \n");
    
    return status;
}



vl_status AudioDevMgr::stopRecord(int handler)
{
    vl_status status = VL_SUCCESS;
    OSStatus result;
    printf("AudioDevMgr opensl stop record ...\n\n");
    
    if(handler != recordId) {
        printf("record id is error!!!!!!!!!!!!!\n");
        return VL_ERR_AUDDEV_ID;
    }
    
    
//    if(mRecordQueue == NULL)
    if(audioUnit == NULL)
    {
        return VL_ERR_AUDDEV_REC_NOT_READY;
        printf("the queue is NULL!!!!!!!!======\n");
    }
    recordMarkStop();
    return status;
}


vl_status AudioDevMgr::releaseRecorder(int handler)
{
    printf("releaseRecorder`````````````\n");
    printf("AudioDevMgr opensl release record, handle=%d\n", handler);
    
    if(handler != recordId) {
        printf("AudioDevMgr release recorder handler=%d, recordid=%d\n", handler, recordId);
        return VL_ERR_AUDDEV_ID;
    }
    
//    AudioQueueFlush(mRecordQueue);
//    OSStatus stopRes=AudioQueueStop(mRecordQueue, TRUE);
//    printf("--------%d---【播放器】-----AudioQueueFlush  \n",stopRes);
    
    // 使用AudioOutputUnitStop停止录制的AudioUnit
    OSStatus stopRes = AudioOutputUnitStop(audioUnit);
    if (stopRes != noErr) {
        printf("---【播放器】----- 结束错误-----\n");
    }
    
    if (stopRes ==0) {
        
        
//        for (int i = 0; i < NUM_BUFFERS; i++) {
////            AudioQueueFreeBuffer(mRecordQueue, mRecordBuffers[i]);
//            AudioUnitUninitialize(audioUnit);
//        }
        // 在释放 bufferList 之前，先对 audioUnit 进行未初始化操作
        AudioUnitUninitialize(audioUnit);
        
        // 释放 AudioUnit 相关资源
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = NULL;
        
        /**
         12/10/ 通过c++ 调用c播放停止按钮的声音提示。。
         */
        UInt32 audioRouteOverride = 1;
        AudioSessionSetProperty (kAudioSessionProperty_OverrideCategoryDefaultToSpeaker, sizeof (audioRouteOverride),
                                 &audioRouteOverride);

                time_t stoptime;
                time(&stoptime);
                char buff[100];
                sprintf(buff, "%ld",stoptime);
                TestObject().playcloseFunction(buff);
//                printf("【播放器】---playcloseFunction--通知播放本地语音");
        
        
        
    }else{
        printf("    ---【播放器】----- 结束错误-----");
        
        AudioUnitUninitialize(audioUnit);
        
        // 释放 AudioUnit 相关资源
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = NULL;
    }
    
    
//    AudioQueueDispose(mRecordQueue, FALSE);
//    mRecordQueue = NULL;
    // 释放录制的AudioUnit对象
//    AudioComponentInstanceDispose(audioUnit);
//    audioUnit = NULL;
    putRecorder(handler);
    
    return VL_SUCCESS;
}

vl_bool AudioDevMgr::isRecording() const {
    vl_bool recording = VL_FALSE;
    
    
    
    return recording;
}

vl_status AudioDevMgr::recordDevInitial(const RecordParameter& param) {
    
    
//    recordBufferSize = SIZE_pbSize;

    return VL_SUCCESS;
}

vl_status AudioDevMgr::recordDevRelease() {
    vl_status ret = VL_SUCCESS;
    
    return ret;
}


