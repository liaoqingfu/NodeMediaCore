//
//  nm_av_codec.h
//  NodeMediaClient
//
//  Created by Mingliang Chen on 15/11/25.
//  Copyright © 2015年 Mingliang Chen. All rights reserved.
//

#ifndef nm_av_codec_h
#define nm_av_codec_h

#include <stdio.h>
#include <stdlib.h>

#define MAX_EXTRA 3

typedef void* hnd_t;

typedef struct {
    /**
     * 从FlvTagAudio中解析出的声道数
     */
    int request_channels;
    
    /**
     *
     */
    int sample_rate;
    int channels;
    int frame_size;
    int buffer_size;
    int audio_profile;
    int audio_bitrate;
    int denoise;
    
    
    int stride;
    int width;
    int height;
    int picture_size;
    int fps;
    int gop;
    int video_profile;
    int video_bitrate;
    int videoRotation;
    
    int src_width;
    int src_height;
    
    int tmp_width;
    int tmp_height;
    
    hnd_t handle;
    
    uint32_t extradata_size;
    uint8_t* extradata;
    
    int    linesize[8];
    
}nm_av_opt;

typedef struct {
    int codec_id;
    
    const char* codec_name;
    
    int (*open_codec)(nm_av_opt* opt);
    
    int (*decode_frame)(nm_av_opt* opt, uint8_t* inData, uint32_t inDataSize, uint8_t** outData);
    
    int (*encode_frame)(nm_av_opt* opt, uint8_t* inData, uint8_t** outData, uint32_t* outDataSize);
    
    int (*close_codec)(nm_av_opt* opt);
    
}nm_av_codec_t;


extern const nm_av_codec_t adc_aac;
extern const nm_av_codec_t aec_aac;

extern const nm_av_codec_t adc_nel;

extern const nm_av_codec_t adc_spx;
extern const nm_av_codec_t aec_spx;

extern const nm_av_codec_t vdc_avc;
extern const nm_av_codec_t vec_avc;


#endif /* nm_av_codec_h */
