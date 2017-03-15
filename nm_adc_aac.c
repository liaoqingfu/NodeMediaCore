//
//  nm_adc_aac.c
//  NodeMediaClient
//
//  Created by Mingliang Chen on 15/11/26.
//  Copyright © 2015年 Mingliang Chen. All rights reserved.
//

#include "nm_common.h"
#include <fdk-aac/aacdecoder_lib.h>

#define DMX_ANC_BUFFSIZE       128
#define DECODER_MAX_CHANNELS     8
#define DECODER_BUFFSIZE      2048 * sizeof(INT_PCM)
#define MAX_LAYERS               2

typedef struct {
    HANDLE_AACDECODER handle;
    uint8_t *decoder_buffer;
    int decoder_buffer_size;
    uint8_t *anc_buffer;
    int conceal_method;
    int drc_level;
    int drc_boost;
    int drc_heavy;
    int drc_cut;
    int level_limit;
} hdn_adc_aac_t;


static int open_codec_aac(nm_av_opt* opt) {
    hdn_adc_aac_t* s = (hdn_adc_aac_t*)calloc(1, sizeof(hdn_adc_aac_t));
    s->handle =  aacDecoder_Open(opt->extradata_size ? TT_MP4_RAW : TT_MP4_ADTS, 1);
    opt->handle = s;
    
    AAC_DECODER_ERROR err;
    if (opt->extradata_size) {
        if ((err = aacDecoder_ConfigRaw(s->handle, &opt->extradata, &opt->extradata_size)) != AAC_DEC_OK) {
            printf("[aac] Unable to set extradata %d\n",err);
            return AVERROR_INVALIDDATA;
        }
    }
    
    if ((err = aacDecoder_SetParam(s->handle, AAC_CONCEAL_METHOD,
                                   s->conceal_method)) != AAC_DEC_OK) {
        printf("[aac] Unable to set error concealment method\n");
        return AVERROR_UNKNOWN;
    }
    
    if (s->drc_boost != -1) {
        if (aacDecoder_SetParam(s->handle, AAC_DRC_BOOST_FACTOR, s->drc_boost) != AAC_DEC_OK) {
             printf("[aac] Unable to set DRC boost factor in the decoder\n");
            return AVERROR_UNKNOWN;
        }
    }
    
    if (s->drc_cut != -1) {
        if (aacDecoder_SetParam(s->handle, AAC_DRC_ATTENUATION_FACTOR, s->drc_cut) != AAC_DEC_OK) {
             printf("[aac] Unable to set DRC attenuation factor in the decoder\n");
            return AVERROR_UNKNOWN;
        }
    }
    
    if (s->drc_level != -1) {
        if (aacDecoder_SetParam(s->handle, AAC_DRC_REFERENCE_LEVEL, s->drc_level) != AAC_DEC_OK) {
             printf("[aac] Unable to set DRC reference level in the decoder\n");
            return AVERROR_UNKNOWN;
        }
    }
    
    if (s->drc_heavy != -1) {
        if (aacDecoder_SetParam(s->handle, AAC_DRC_HEAVY_COMPRESSION, s->drc_heavy) != AAC_DEC_OK) {
             printf("[aac] Unable to set DRC heavy compression in the decoder\n");
            return AVERROR_UNKNOWN;
        }
    }
    
#ifdef AACDECODER_LIB_VL0
    if (aacDecoder_SetParam(s->handle, AAC_PCM_LIMITER_ENABLE, s->level_limit) != AAC_DEC_OK) {
         printf("[aac] Unable to set in signal level limiting in the decoder\n");
        return AVERROR_UNKNOWN;
    }
#endif
    
    
    s->decoder_buffer_size = DECODER_BUFFSIZE * DECODER_MAX_CHANNELS;
    s->decoder_buffer= malloc(s->decoder_buffer_size);
    return 0;
}


static int decode_frame_aac(nm_av_opt* opt, uint8_t* inData, uint32_t inDataSize, uint8_t** outData) {
    hdn_adc_aac_t* s = (hdn_adc_aac_t*)opt->handle;
    AAC_DECODER_ERROR err;
    UCHAR* inBuffer[MAX_LAYERS];
    UINT inBufferLength[MAX_LAYERS] = {0};
    UINT bytesValid = (UINT)inDataSize;
    inBuffer[0] =inData;
    inBufferLength[0] = (UINT)inDataSize;
    
    err = aacDecoder_Fill(s->handle, inBuffer, inBufferLength, &bytesValid);
    if (err != AAC_DEC_OK) {
        printf("[aac] aacDecoder_Fill() failed: %x\n", err);
        return AVERROR_INVALIDDATA;
    }
    
    err = aacDecoder_DecodeFrame(s->handle, (INT_PCM *) s->decoder_buffer, s->decoder_buffer_size, 0);
    if (err == AAC_DEC_NOT_ENOUGH_BITS) {
        printf("[aac] aacDecoder_DecodeFrame() net enough bits: %x\n", err);
        return AVERROR_INVALIDDATA;
    }
    if (err != AAC_DEC_OK) {
        printf("[aac] aacDecoder_DecodeFrame() failed: %x\n", err);
        return AVERROR_UNKNOWN;
    }
    if(opt->sample_rate == 0) {
        CStreamInfo *info     = aacDecoder_GetStreamInfo(s->handle);
        opt->channels = info->numChannels;
        opt->sample_rate = info->sampleRate;
        opt->frame_size = info->frameSize;
        opt->buffer_size = opt->frame_size * opt->channels * 2;
    }
    
    *outData = malloc(opt->buffer_size);
    memcpy(*outData, s->decoder_buffer, opt->buffer_size);
    return inDataSize;
}

static int close_codec_aac(nm_av_opt* opt) {
    hdn_adc_aac_t* s = (hdn_adc_aac_t*)opt->handle;
    if(s->handle) {
        aacDecoder_Close(s->handle);
    }
    freep(s->decoder_buffer);
    freep(s->anc_buffer);
    freep(opt->extradata);
    freep(opt->handle);
    return 0;
}


const nm_av_codec_t adc_aac = {
    .codec_id   = 10,
    .open_codec = open_codec_aac,
    .decode_frame = decode_frame_aac,
    .encode_frame = NULL,
    .close_codec = close_codec_aac
};