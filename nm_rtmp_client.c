//
// Created by Mingliang Chen on 15/11/25.
//

#include "nm_rtmp_client.h"
#include "nm_common.h"
#include "nm_av_codec.h"
static uint8_t NALUH[4] = {0,0,0,1};
static int SOUNDRATE[4] = {5500,11025,22050,44100};

static void EventCallback(int env,const char* msg) {
    
}

static void VideoInfoCallback(int width, int height,int buffer_size) {
    //printf("VideoInfoCallback width:%d height:%d buffer_size:%d\n",width, height,buffer_size);
}

static void AudioInfoCallback(int sample_rate, int channels, int buffer_size) {
    //printf("AudioInfoCallback sample_rate:%d channels:%d buffer_size:%d\n",sample_rate, channels, buffer_size);
}

static void VideoDataCallback(uint8_t *data, int size) {
    
}

static void AudioDataCallback(uint8_t *data, int size) {
    
}

static void TsCallback(uint8_t *data, int size) {
    
}

static void LogCallback(int level, const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    //printf(fmt,vl);
    va_end(vl);
}



static void* AudioRenderThread(void* ctx) {
    nm_rtmp_client_t* rp =  (nm_rtmp_client_t*)ctx;
    rp->logCb(0, "AudioRenderThread start.");
    Buffer* buffer = NULL;
    
    int64_t sys_clk_cur = 0;
    int64_t pts_clk_cur = 0;
    int64_t sys_clk_dif = 0;
    int64_t pts_clk_dif = 0;
    
    while (!rp->abort) {
        if (buffer == NULL) {
            if (rp->streamStatus == STREAM_BUFFER_FULL && FFMAX(rp->audioOuQueue.nb_packets,rp->videoOuQueue.nb_packets) == 0 && !rp->haveVideo) {
                rp->streamStatus = STREAM_BUFFER_EMPTY;
                rp->logCb(0, "NetStream.Buffer.Empty");
                rp->eventCb(1100, "NetStream.Buffer.Empty");
            }
            
            if (buffer_queue_get(&rp->audioOuQueue, &buffer, true) == -1) {
                usleep(10 * 1000);
                continue;
            }
        }
        if(rp->streamStatus == STREAM_BUFFER_EMPTY && !rp->haveVideo) {
            rp->streamStatus = STREAM_BUFFER_BUFFERING;
            rp->logCb(0, "NetStream.Buffer.Buffering");
            rp->eventCb(1101, "NetStream.Buffer.Buffering");
        }
        sys_clk_cur = gettime();
        pts_clk_cur = buffer->time_stamp;
        if(rp->sys_clk_sta == 0 && rp->pts_clk_sta == 0 && !rp->haveVideo) {
//            rp->logCb(1,"sys_clk_sta:%lld pts_clk_sta:%lld\n",sys_clk_cur,pts_clk_cur);
            rp->sys_clk_sta = sys_clk_cur + rp->bufferTime;
            rp->pts_clk_sta = pts_clk_cur ;
        }
        
        sys_clk_dif = sys_clk_cur - rp->sys_clk_sta;
        pts_clk_dif = pts_clk_cur - rp->pts_clk_sta;
//        rp->logCb(1,"sys_clk_dif:%lld pts_clk_dif:%lld\n",sys_clk_dif,pts_clk_dif);
        
        if (pts_clk_dif < sys_clk_dif) {
            if(rp->streamStatus == STREAM_BUFFER_BUFFERING && !rp->haveVideo) {
                rp->streamStatus = STREAM_BUFFER_FULL;
                rp->logCb(0, "NetStream.Buffer.Full");
                rp->eventCb(1102, "NetStream.Buffer.Full");
            }
            rp->audioDataCb(buffer->data, buffer->data_size);
            buffer_free(buffer);
            buffer = NULL;
            continue;
        }

        usleep(10000);
    }
    
    rp->audioOverCb();
    rp->logCb(0, "AudioRenderThread stop.");
    return NULL;
}

static void* VideoRenderThread(void* ctx) {
    nm_rtmp_client_t* rp =  (nm_rtmp_client_t*)ctx;
    rp->logCb(0, "VideoRenderThread start.");
    Buffer* buffer = NULL;
    int64_t sys_clk_cur = 0;
    int64_t pts_clk_cur = 0;
    int64_t sys_clk_dif = 0;
    int64_t pts_clk_dif = 0;
    
    while (!rp->abort) {
        if (buffer == NULL) {
            if (rp->streamStatus == STREAM_BUFFER_FULL && FFMAX(rp->audioOuQueue.nb_packets,rp->videoOuQueue.nb_packets) == 0) {
                rp->streamStatus = STREAM_BUFFER_EMPTY;
                rp->logCb(0, "NetStream.Buffer.Empty");
                rp->eventCb(1100, "NetStream.Buffer.Empty");
            }
            
            if (buffer_queue_get(&rp->videoOuQueue, &buffer, true) == -1) {
                usleep(10 * 1000);
                continue;
            }
        }
        
        if(rp->streamStatus == STREAM_BUFFER_EMPTY) {
            rp->streamStatus = STREAM_BUFFER_BUFFERING;
            rp->logCb(0, "NetStream.Buffer.Buffering");
            rp->eventCb(1101, "NetStream.Buffer.Buffering");
        }
        
        sys_clk_cur = gettime();
        pts_clk_cur = buffer->time_stamp;

        if(rp->sys_clk_sta == 0 && rp->pts_clk_sta == 0 ) {
            rp->sys_clk_sta = sys_clk_cur + rp->bufferTime;
            rp->pts_clk_sta = pts_clk_cur;
        }
        sys_clk_dif = sys_clk_cur - rp->sys_clk_sta;
        pts_clk_dif = pts_clk_cur - rp->pts_clk_sta;
        
        if (pts_clk_dif < sys_clk_dif) {
            if(rp->streamStatus == STREAM_BUFFER_BUFFERING) {
                rp->streamStatus = STREAM_BUFFER_FULL;
                rp->logCb(0, "NetStream.Buffer.Full");
                rp->eventCb(1102, "NetStream.Buffer.Full");
            }
            rp->videoDataCb(buffer->data, buffer->data_size);
            buffer_free(buffer);
            buffer = NULL;
            continue;
        }
        
        usleep(10000);
    }
    rp->videoOverCb();
    rp->logCb(0, "VideoRenderThread stop.");
    return NULL;
}


void* AudioDecodeThread(void* ctx) {
    nm_rtmp_client_t* nrc =  (nm_rtmp_client_t*)ctx;
    nrc->logCb(1,"Audio decode thread start ...\n");
    Buffer* audioInBuffer = NULL;
    nm_av_codec_t codec;
    
    int ret = 0;
    bool isInfoCallback = false;
    int rawDataPos = 1;
    
    uint8_t* pcmData = NULL;

    pthread_t audioRenderThreadId = 0;
    while (!nrc->abort) {
        if(buffer_queue_get(&nrc->audioInQueue, &audioInBuffer, true) == -1) {
            usleep(10*1000);
            continue;
        }
        if(!nrc->haveAudio) {
            //1.从FlvTagAudio中解析出音频编码格式
            char soundFormat = srs_utils_flv_audio_sound_format((char*)audioInBuffer->data, audioInBuffer->data_size);
            char sampleRate  = srs_utils_flv_audio_sound_rate((char*)audioInBuffer->data, audioInBuffer->data_size);
            //2.注册解码器
            switch (soundFormat) {
                case 1:
                    //ADPCM
                    break;
                case 2:
                    //mp3
                    break;
                case 4:
                    //nellymoser 16k
                    codec = adc_nel;
                    rawDataPos = 1;
                    nrc->audio_opt.sample_rate = 16000;
                    nrc->audio_opt.channels = 1;
                    nrc->audio_opt.frame_size = 256;
                    nrc->audio_opt.buffer_size = 512;
                    nrc->logCb(1,"Find audio decoder: Nellymoser 16k.\n");
                    break;
                case 5:
                    //nellymoser 8k
                    codec = adc_nel;
                    rawDataPos = 1;
                    nrc->audio_opt.sample_rate = 8000;
                    nrc->audio_opt.channels = 1;
                    nrc->audio_opt.frame_size = 256;
                    nrc->audio_opt.buffer_size = 512;
                    nrc->logCb(1,"Find audio decoder: Nellymoser 8k.\n");
                    break;
                case 6:
                    //nellymoser
                    codec = adc_nel;
                    rawDataPos = 1;
                    nrc->audio_opt.sample_rate = SOUNDRATE[sampleRate];
                    nrc->audio_opt.channels = 1;
                    nrc->audio_opt.frame_size = 256;
                    nrc->audio_opt.buffer_size = 512;
                    nrc->logCb(1,"Find audio decoder: Nellymoser %d.\n",nrc->audio_opt.sample_rate);
                    break;
                case 10:
                    //aac
                    codec = adc_aac;
                    rawDataPos = 2;
                    if(!srs_utils_flv_audio_aac_packet_type((char*)audioInBuffer->data, audioInBuffer->data_size)) {
                        nrc->audio_opt.extradata_size = audioInBuffer->data_size - rawDataPos;
                        nrc->audio_opt.extradata = malloc(nrc->audio_opt.extradata_size );
                        memcpy(nrc->audio_opt.extradata, audioInBuffer->data + rawDataPos, nrc->audio_opt.extradata_size);
                    }
                    nrc->logCb(1,"Find audio decoder: aac.\n");
                    break;
                case 11:
                    //speex
                    codec = adc_spx;
                    rawDataPos = 1;
                    nrc->audio_opt.sample_rate = 16000;
                    nrc->audio_opt.channels = 1;
                    nrc->audio_opt.frame_size = 320;
                    nrc->audio_opt.buffer_size = 640;
                    nrc->logCb(1,"Find audio decoder: speex.\n");
                    break;
                default:
                    break;
            }
            
            //3.无法找到解码器，不处理音频
            if(codec.codec_id == 0) {
                nrc->logCb(1,"Can not find audio decoder id:%d.\n",soundFormat);
                nrc->processAudio = false;
                continue;
            }
            
            //4.打开音频解码器
            codec.open_codec(&nrc->audio_opt);
            
            
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
            pthread_create(&audioRenderThreadId, &attr, AudioRenderThread, nrc);
            buffer_queue_init(&nrc->audioOuQueue);
            
            nrc->haveAudio = true;
        } else {
            //5.解码音频
            uint8_t* packet_data = audioInBuffer->data+rawDataPos;
            int      packet_size = audioInBuffer->data_size-rawDataPos;
            do {
                ret = codec.decode_frame(&nrc->audio_opt, packet_data, packet_size, &pcmData);
                if( ret >= 0 ) {
                    //6.判断是否需要回调音频参数
                    if(!isInfoCallback) {
                        nrc->audioInfoCb(nrc->audio_opt.sample_rate, nrc->audio_opt.channels, nrc->audio_opt.buffer_size);
                        isInfoCallback = true;
                    }
                    
                    //7.解码后的音频数据再次进入渲染队列
                    Buffer *buffer = buffer_alloc(pcmData, nrc->audio_opt.buffer_size, audioInBuffer->time_stamp);
                    buffer_queue_put(&nrc->audioOuQueue, buffer);
                    freep(pcmData);
                }
                packet_data += ret;
                packet_size -= ret;
                
            }while(packet_size > 0 );
            
            
            
        }
        buffer_free(audioInBuffer);
        
    }
    
    buffer_queue_abort(&nrc->audioOuQueue);
    nrc->logCb(1,"Wait audio render thread ...\n");
    
    buffer_queue_destroy(&nrc->audioOuQueue);
    if(nrc->haveAudio) {
        pthread_join(audioRenderThreadId, NULL);
        codec.close_codec(&nrc->audio_opt);
        nrc->audioOverCb();
    }
    nrc->logCb(1,"Audio decode thread stop.\n");
    return NULL;
}

void* VideoDecodeThread(void* ctx) {
    nm_rtmp_client_t* nrc =  (nm_rtmp_client_t*)ctx;
    nrc->logCb(1,"Video decode thread start ...\n");
    Buffer *videoInBuffer = NULL;
    
    nm_av_codec_t codec;
    uint8_t* sps = NULL;
    uint8_t* pps = NULL;
    uint32_t spsLen = 0;
    uint32_t ppsLen = 0;
    
    uint8_t* NALU = NULL;
    int naluSize = 0;
    uint8_t* yuvData = NULL;
    
    bool isInfoCallback = false;
    
    pthread_t videoRenderThreadId = 0;
    H264BSFContext bs_ctx = {0};
    while (!nrc->abort) {
        if(buffer_queue_get(&nrc->videoInQueue, &videoInBuffer, true) == -1) {
            usleep(10*1000);
            continue;
        }
        int CodecID = srs_utils_flv_video_codec_id((char*)videoInBuffer->data, videoInBuffer->data_size);
        int AVCPacketType = srs_utils_flv_video_avc_packet_type((char*)videoInBuffer->data, videoInBuffer->data_size);
        //        int FrameType = srs_utils_flv_video_frame_type((char*)videoInBuffer->data, videoInBuffer->data_size);
        if(!nrc->haveVideo) {
            if(CodecID == 7 && AVCPacketType == 0) {
                //AVC sequence header 解析出sps pps
                int ret = AVCDecoderConfigurationRecord(videoInBuffer->data + 5,videoInBuffer->data_size - 5, &sps, &spsLen, &pps, &ppsLen);
                if(ret != 0) {
                    nrc->logCb(1,"Can not parser AVC sequence header.\n");
                }
                nrc->logCb(1,"Parser sps/pps int AVC sequence header.\n");
//                dumpBuffer(sps, spsLen);
//                dumpBuffer(pps, ppsLen);
                nrc->video_opt.extradata_size = 4 + spsLen + 4 + ppsLen;
                nrc->video_opt.extradata = malloc(nrc->video_opt.extradata_size );
                memcpy(nrc->video_opt.extradata, NALUH, 4);
                memcpy(nrc->video_opt.extradata + 4, sps, spsLen);
                memcpy(nrc->video_opt.extradata + 4 + spsLen, NALUH, 4);
                memcpy(nrc->video_opt.extradata + 4 + spsLen + 4, pps, ppsLen);
                freep(sps);
                freep(pps);
                codec = vdc_avc;
            }
            
            if(codec.codec_id == 0) {
                nrc->logCb(1,"Can not find video decoder id:%d.\n",CodecID);
                nrc->processVideo = false;
                continue;
            }
            
            //4.打开解码器
            codec.open_codec(&nrc->video_opt);
            codec.decode_frame(&nrc->video_opt, nrc->video_opt.extradata, nrc->video_opt.extradata_size, &yuvData);
            
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
            pthread_create(&videoRenderThreadId, &attr, VideoRenderThread, nrc);
            buffer_queue_init(&nrc->videoOuQueue);

            
            nrc->haveVideo = true;
            
        }else {
            if(CodecID == 7 && AVCPacketType == 1) {
                int ret = 0;
                naluSize = videoInBuffer->data_size - 5;
                NALU = videoInBuffer->data + 5;
                uint8_t *poutbuf = NULL;
                int poutbuf_size = 0 ;
                nm_h264_mp4toannexb_filter(&bs_ctx,&poutbuf, &poutbuf_size, NALU, naluSize);    
                ret = codec.decode_frame(&nrc->video_opt, poutbuf, poutbuf_size, &yuvData);
                freep(poutbuf);
                if(ret == 0) {
                    if( !isInfoCallback ) {
                        nrc->videoInfoCb(nrc->video_opt.width, nrc->video_opt.height,nrc->video_opt.width * nrc->video_opt.height * 3 / 2);
                        isInfoCallback = true;
                    }
                    
                    Buffer *buffer = buffer_alloc(yuvData, nrc->video_opt.picture_size, videoInBuffer->time_stamp);
                    buffer_queue_put(&nrc->videoOuQueue, buffer);
                    freep(yuvData);
                }
            }
        }
        
        
        
        buffer_free(videoInBuffer);
        
    }
    
    buffer_queue_abort(&nrc->videoOuQueue);
    nrc->logCb(1,"Wait video render thread ...\n");
    buffer_queue_destroy(&nrc->videoOuQueue);
    if(nrc->haveVideo) {
        pthread_join(videoRenderThreadId, NULL);
        codec.close_codec(&nrc->video_opt);
        nrc->videoOverCb();
    }
    nrc->logCb(1,"Video decode thread stop.\n");
    
    return NULL;
}

#ifndef AV_RB16
#   define AV_RB16(x)                           \
((((const uint8_t*)(x))[0] << 8) |          \
((const uint8_t*)(x))[1])
#endif

int parese_result(char type, int size, char* data) {
    int ret = 0;
    if(type == 4) {
        //handele ping
        if(size == 6) {
            int t = AV_RB16(data);
            if(t == 0) {
                //STREAM BEGIN
                //printf("Stream Begin.\n");
            } else if(t == 1) {
                //STREAM EOF
                //printf("Stream EOF.\n");
                ret = -1;
            }
        }
    }else if(type == 20) {
        //handle invoke
        int pos = 0;
        int nparsed = 0;
        do {
            srs_amf0_t amfData = srs_amf0_parse(data+pos,size-pos,&nparsed);
            pos += nparsed;
            if(srs_amf0_is_object(amfData)) {
                for(int i=0;i<srs_amf0_object_property_count(amfData);i++) {
                    if( !strcmp(srs_amf0_object_property_name_at(amfData, i), "code")) {
                        srs_amf0_t codeAmf =srs_amf0_object_property_value_at(amfData, i);
                        //printf("onStatus: %s\n",srs_amf0_to_string(codeAmf));
                        if(!strcmp( srs_amf0_to_string(codeAmf),"NetStream.Play.UnpublishNotify")) {
                            ret = -1;
                        }
                    }
                }
            }
            srs_amf0_free(amfData);
        }while ( pos < size);
    }
    return ret;
}

void* PlayThread(void* ctx) {
    nm_rtmp_client_t* nrc =  (nm_rtmp_client_t*)ctx;
    nrc->logCb(1,"Play thread start ...\n");
    nrc->eventCb(1000,"Connecting to stream.");
    pthread_t audioDecodeThreadId;
    pthread_t videoDecodeThreadId;
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    //TODO 通过参数判断来创建不同的线程
    //软件解码就创建两个解码线程
    //硬件解码就创建一个合流为mpegts线程
    pthread_create(&audioDecodeThreadId, &attr, AudioDecodeThread, nrc);
    pthread_create(&videoDecodeThreadId, &attr, VideoDecodeThread, nrc);
    
    
    nrc->streamStatus = STREAM_CONNECTION;
    do {
        int ret = srs_rtmp_handshake(nrc->rtmp);
        if(ret != 0) {
            nrc->logCb(2,"rtmp_handshake error.\n");
            nrc->eventCb(1002,"rtmp handshake error.");
            break;
        }
        ret = srs_rtmp_connect_app(nrc->rtmp);
        if(ret != 0) {
            nrc->logCb(2,"rtmp_connect_app error.\n");
            nrc->eventCb(1002,"rtmp connect app error.");
            break;
        }
        ret = srs_rtmp_play_stream(nrc->rtmp);
        if(ret != 0) {
            nrc->logCb(2,"rtmp_play_stream error.\n");
            nrc->eventCb(1002,"rtmp play stream error.");
            break;
        }
        nrc->streamStatus = STREAM_CONNECTED;
        nrc->eventCb(1001, "NetConnection.Connect.Success");
        
        buffer_queue_init(&nrc->audioInQueue);
        buffer_queue_init(&nrc->videoInQueue);
        
        nrc->streamStatus = STREAM_BUFFER_EMPTY;
        nrc->abort = false;
        while (!nrc->abort) {
            int size;
            char type;
            char* data;
            uint32_t timestamp;
            if (FFMAX(nrc->audioOuQueue.buffer_length,nrc->videoOuQueue.buffer_length) > nrc->bufferTime * 2) {
                usleep(100000);
                continue;
            }
            if (srs_rtmp_read_packet(nrc->rtmp, &type, &timestamp, &data, &size) != 0) {
                break;
            }
            if(type == SRS_RTMP_TYPE_AUDIO) {
                Buffer *packet = buffer_alloc((uint8_t*)data, size, timestamp);
                buffer_queue_put(&nrc->audioInQueue, packet);
            } else if(type == SRS_RTMP_TYPE_VIDEO) {
                Buffer *packet = buffer_alloc((uint8_t*)data, size, timestamp);
                buffer_queue_put(&nrc->videoInQueue, packet);
            } else {
                if(parese_result(type, size, data) == -1 ){
                    do{
                        if(nrc->abort) {
                            break;
                        }
                        usleep(100000);
                    }while(FFMAX(nrc->audioOuQueue.nb_packets, nrc->videoOuQueue.nb_packets) > 0);
                    nrc->eventCb(1103, "Stream EOF");
                    nrc->abort = true;
                }
            }
            
            free(data);
        }
        if(!nrc->abort) {
            nrc->abort = true;
            nrc->eventCb(1005,"");
        }
    } while (0);
    nrc->logCb(1,"Play over, start release ...\n");
    
    if(nrc->rtmp && nrc->streamStatus >= STREAM_CONNECTED) {
        srs_rtmp_destroy(nrc->rtmp);
        nrc->rtmp = NULL;
    }
    
    buffer_queue_abort(&nrc->audioInQueue);
    nrc->logCb(1,"Wait audio Decode thread ...\n");
    pthread_join(audioDecodeThreadId, NULL);
    buffer_queue_destroy(&nrc->audioInQueue);
    
    buffer_queue_abort(&nrc->videoInQueue);
    nrc->logCb(1,"Wait video Decode thread ...\n");
    pthread_join(videoDecodeThreadId, NULL);
    buffer_queue_destroy(&nrc->videoInQueue);
    
    nrc->streamStatus = STREAM_CLOSE;
    nrc->logCb(1,"Play thread stop.\n");
    nrc->eventCb(1004, "NetConnection.Connect.Closed");
    return NULL;
}

void* PublishThread(void* ctx) {
    nm_rtmp_client_t* nrc =  (nm_rtmp_client_t*)ctx;
    nrc->logCb(1,"Publish thread start ...\n");
    nrc->eventCb(2000, "Start open output.");
    nrc->streamStatus = STREAM_CONNECTION;
    do {
        buffer_queue_init(&nrc->mediaOuQueue);
        
        int ret = srs_rtmp_handshake(nrc->rtmp);
        if(ret != 0) {
            nrc->logCb(2,"rtmp_handshake error.\n");
            nrc->eventCb(2002,"rtmp handshake error.");
            break;
        }
        ret = srs_rtmp_connect_app(nrc->rtmp);
        if(ret != 0) {
            nrc->logCb(2,"rtmp_connect_app error.\n");
            nrc->eventCb(2002,"rtmp connect app error.");
            break;
        }
        
        ret = srs_rtmp_publish_stream(nrc->rtmp);
        if(ret != 0) {
            nrc->logCb(2,"rtmp_publish_stream error.\n");
            nrc->eventCb(2002,"rtmp publish stream error.");
            break;
        }
        
        
        srs_amf0_t setString = srs_amf0_create_string("@setDataFrame");
        int setStringSize = srs_amf0_size(setString);
        char* data0 = malloc(setStringSize);
        srs_amf0_serialize(setString,data0,setStringSize);
        
        
        srs_amf0_t metaString = srs_amf0_create_string("onMetaData");
        int metaStringSize = srs_amf0_size(metaString);
        char* data1 = malloc(metaStringSize);
        srs_amf0_serialize(metaString,data1,metaStringSize);
        
        srs_amf0_t eArray = srs_amf0_create_ecma_array();
        srs_amf0_t durationValue = srs_amf0_create_number(0);
        srs_amf0_t widthValue = srs_amf0_create_number(nrc->video_opt.width);
        srs_amf0_t heightValue = srs_amf0_create_number(nrc->video_opt.height);
        srs_amf0_t framerateValue = srs_amf0_create_number(nrc->video_opt.fps);
        srs_amf0_t vb = srs_amf0_create_number(nrc->video_opt.video_bitrate / 1000 );
        srs_amf0_t ab = srs_amf0_create_number(nrc->audio_opt.audio_bitrate / 1000);
        srs_amf0_t vid = srs_amf0_create_number(7);
        srs_amf0_t aid = srs_amf0_create_number(10);
        srs_amf0_t asr = srs_amf0_create_number(44100);
        srs_amf0_t ass = srs_amf0_create_number(16);
        
        
        srs_amf0_ecma_array_property_set(eArray,"duration",durationValue);
        srs_amf0_ecma_array_property_set(eArray,"width",widthValue);
        srs_amf0_ecma_array_property_set(eArray,"height",heightValue);
        srs_amf0_ecma_array_property_set(eArray,"framerate",framerateValue);
        srs_amf0_ecma_array_property_set(eArray,"videocodecid",vid);
        srs_amf0_ecma_array_property_set(eArray,"videodatarate",vb);
        srs_amf0_ecma_array_property_set(eArray,"audiodatarate",ab);
        srs_amf0_ecma_array_property_set(eArray,"audiocodecid",aid);
        srs_amf0_ecma_array_property_set(eArray,"audiosamplerate",asr);
        srs_amf0_ecma_array_property_set(eArray,"audiosamplesize",ass);
        
        int eArraySize = srs_amf0_size(eArray);
        char* data2 = malloc(eArraySize);
        srs_amf0_serialize(eArray,data2,eArraySize);
        
        int metaPacketSize =setStringSize+ metaStringSize+ eArraySize;
        char* metaPacket = malloc(metaPacketSize);
        memcpy(metaPacket, data0, setStringSize);
        memcpy(metaPacket + setStringSize, data1, metaStringSize);
        memcpy(metaPacket + setStringSize + metaStringSize, data2, eArraySize);
        
        
        srs_rtmp_write_packet(nrc->rtmp, SRS_RTMP_TYPE_SCRIPT, 0, metaPacket, metaPacketSize);
        
        srs_amf0_free(setString);
        srs_amf0_free(metaString);
        srs_amf0_free(eArray);
        
        free(data0);
        free(data1);
        free(data2);

        if (nrc->haveVideo) {
            nrc->video_encoder = vec_avc;
            nrc->video_encoder.open_codec(&nrc->video_opt);
            srs_h264_write_raw_frames(nrc->rtmp, (char*)nrc->video_opt.extradata, nrc->video_opt.extradata_size, 0, 0);
        }
        
        if (nrc->haveAudio) {
            nrc->preprocess_state = speex_preprocess_state_init(nrc->audio_opt.frame_size, nrc->audio_opt.sample_rate);
            int state = 1;
            speex_preprocess_ctl(nrc->preprocess_state, SPEEX_PREPROCESS_SET_DENOISE, &state);
            
            nrc->audio_encoder = aec_aac;
            nrc->audio_encoder.open_codec(&nrc->audio_opt);
            char *audioPacket = malloc(nrc->audio_opt.extradata_size + 2);;
            audioPacket[0] = 0xAF;
            audioPacket[1] = 0x00;
            memcpy(audioPacket+2, nrc->audio_opt.extradata, nrc->audio_opt.extradata_size);
            if(srs_rtmp_write_packet(nrc->rtmp, SRS_RTMP_TYPE_AUDIO, 0, audioPacket, 2+nrc->audio_opt.extradata_size) != 0 ) {
                freep(audioPacket);
            }
        }
        
        
        nrc->streamStatus = STREAM_CONNECTED;
        nrc->eventCb(2001, "NetConnection.Connect.Success");
        
        int64_t startTs = 0;
        uint32_t pts = 0;
        while (!nrc->abort) {
            Buffer* pkt = NULL;
            if( buffer_queue_get(&nrc->mediaOuQueue, &pkt, true) == -1 ) {
                usleep(10*1000);
                continue;
            }
            
            if(startTs == 0) {
                startTs = pkt->time_stamp;
            }
            pts = (uint32_t)(pkt->time_stamp - startTs);
            if(pts > 0) {
                if(pkt->type == SRS_RTMP_TYPE_AUDIO) {
                    char* audioPacket = malloc(pkt->data_size + 2);
                    audioPacket[0] = 0xAF;
                    audioPacket[1] = 0x01;
                    memcpy(audioPacket + 2, pkt->data, pkt->data_size);
                    if(srs_rtmp_write_packet(nrc->rtmp, SRS_RTMP_TYPE_AUDIO, pts, audioPacket, 2+pkt->data_size) != 0) {
                        freep(audioPacket);
                    }
                }else {
                    srs_h264_write_raw_frames(nrc->rtmp, (char*)pkt->data, pkt->data_size, pts, pts);
                }
            }
            buffer_free(pkt);
        }
        
        
        
    }while(0);
    
    if(nrc->rtmp && nrc->streamStatus >= STREAM_CONNECTED) {
        srs_rtmp_destroy(nrc->rtmp);
        nrc->rtmp = NULL;
    }
    nrc->streamStatus = STREAM_CLOSE;
    
    buffer_queue_abort(&nrc->mediaOuQueue);
    
    if (nrc->preprocess_state) {
        speex_preprocess_state_destroy(nrc->preprocess_state);
        nrc->preprocess_state = NULL;
    }
    
    if(nrc->audio_encoder.close_codec) {
        nrc->audio_encoder.close_codec(&nrc->audio_opt);
    }
    
    if(nrc->video_encoder.close_codec) {
        nrc->video_encoder.close_codec(&nrc->video_opt);
    }
    
    
    
    nrc->logCb(1,"Publish thread stop.\n");
    nrc->eventCb(2004, "NetConnection.Connect.Closed");
    return NULL;
}



nm_rtmp_client_t* nm_rtmp_client_create(const char* rtmpUrl, const char* pageUrl, const char* swfUrl) {
    
    nm_rtmp_client_t* nrc =  (nm_rtmp_client_t*)calloc(1,sizeof(nm_rtmp_client_t));
    
    nrc->rtmp        = srs_rtmp_create(rtmpUrl);
    srs_rtmp_set_connect_args(nrc->rtmp, NULL, swfUrl, pageUrl, NULL);
    
    nrc->eventCb     = EventCallback;
    nrc->audioInfoCb = AudioInfoCallback;
    nrc->videoInfoCb = VideoInfoCallback;
    nrc->audioDataCb = AudioDataCallback;
    nrc->videoDataCb = VideoDataCallback;
    nrc->tsCb        = TsCallback;
    nrc->logCb       = LogCallback;
    
    memset(&nrc->audio_opt, 0, sizeof(nm_av_opt));
    memset(&nrc->video_opt, 0, sizeof(nm_av_opt));
    if(nrc->rtmp == NULL) {
        freep(nrc);
    }
    
    return nrc;
}

void nm_rtmp_client_destroy(nm_rtmp_client_t* nrc) {
    freep(nrc);
}

int nm_rtmp_client_start_play(nm_rtmp_client_t* nrc) {
    if(nrc->streamStatus > STREAM_CLOSE) {
        nrc->logCb(2,"Play error,stream stat:%d\n",nrc->streamStatus);
        return -1;
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&nrc->openThreadId, &attr, PlayThread, nrc);
    return 0;
}

int nm_rtmp_client_stop_play(nm_rtmp_client_t* nrc) {
    if(nrc == 0) {
        return -1;
    }
    if(nrc->rtmp) {
        srs_rtmp_destroy(nrc->rtmp);
        nrc->rtmp = NULL;
    }
    nrc->abort = true;
    pthread_join(nrc->openThreadId, NULL);
    return 0;
}

int nm_rtmp_client_start_publish(nm_rtmp_client_t* nrc) {
    if(nrc->streamStatus > STREAM_CLOSE) {
        nrc->logCb(2,"Publish error,stream stat:%d\n",nrc->streamStatus);
        return -1;
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&nrc->openThreadId, &attr, PublishThread, nrc);
    return 0;
}

int nm_rtmp_client_stop_publish(nm_rtmp_client_t* nrc) {
    if(nrc == 0) {
        return -1;
    }
    if(nrc->rtmp && nrc->streamStatus < STREAM_CONNECTED ) {
        srs_rtmp_destroy(nrc->rtmp);
        nrc->rtmp = NULL;
    }
    nrc->abort = true;
    buffer_queue_abort(&nrc->mediaOuQueue);
    pthread_join(nrc->openThreadId, NULL);
    return 0;
}

int nm_rtmp_client_put_audio(nm_rtmp_client_t* nrc, uint8_t *data, size_t size) {
    if(nrc->streamStatus < STREAM_CONNECTED) {
        return -1;
    }
    uint8_t* outData = NULL;
    uint32_t outDataSize;
    
    if (nrc->audio_opt.denoise) {
        speex_preprocess_run(nrc->preprocess_state, (spx_int16_t*) data);
    }
    
    
    if( nrc->audio_encoder.encode_frame(&nrc->audio_opt, data, &outData, &outDataSize) == 0 ) {
        Buffer *packet = buffer_alloc(outData, outDataSize, (uint32_t)gettime());;
        packet->type = SRS_RTMP_TYPE_AUDIO;
        buffer_queue_put(&nrc->mediaOuQueue, packet);
        freep(outData);
    }
    return 0;
}

int nm_rtmp_client_put_video(nm_rtmp_client_t* nrc, uint8_t *data, size_t size) {
    if(nrc->streamStatus < STREAM_CONNECTED) {
        return -1;
    }
    uint8_t* outData = NULL;
    uint32_t outDataSize;
    
    if(nrc->video_encoder.encode_frame(&nrc->video_opt, data, &outData, &outDataSize) == 0) {
        Buffer *packet = buffer_alloc(outData, outDataSize, (uint32_t)gettime());;
        packet->type = SRS_RTMP_TYPE_VIDEO;
        buffer_queue_put(&nrc->mediaOuQueue, packet);
        freep(outData);
    }
    return 0;
}
