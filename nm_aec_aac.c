//
//  nm_aec_aac.c
//  NodeMediaClient
//
//  Created by Mingliang Chen on 15/11/29.
//  Copyright © 2015年 NodeMedia. All rights reserved.
//


#include "nm_common.h"
#include <fdk-aac/aacenc_lib.h>


static const char *aac_get_error(AACENC_ERROR err)
{
    switch (err) {
        case AACENC_OK:
            return "No error";
        case AACENC_INVALID_HANDLE:
            return "Invalid handle";
        case AACENC_MEMORY_ERROR:
            return "Memory allocation error";
        case AACENC_UNSUPPORTED_PARAMETER:
            return "Unsupported parameter";
        case AACENC_INVALID_CONFIG:
            return "Invalid config";
        case AACENC_INIT_ERROR:
            return "Initialization error";
        case AACENC_INIT_AAC_ERROR:
            return "AAC library initialization error";
        case AACENC_INIT_SBR_ERROR:
            return "SBR library initialization error";
        case AACENC_INIT_TP_ERROR:
            return "Transport library initialization error";
        case AACENC_INIT_META_ERROR:
            return "Metadata library initialization error";
        case AACENC_ENCODE_ERROR:
            return "Encoding error";
        case AACENC_ENCODE_EOF:
            return "End of file";
        default:
            return "Unknown error";
    }
}

typedef struct {
    HANDLE_AACENCODER handle;
    int afterburner;
    int eld_sbr;
    int signaling;
    int latm;
    int header_period;
    int vbr;
} hdn_aec_aac_t;


static int open_codec_aac(nm_av_opt* opt) {
    hdn_aec_aac_t* s = (hdn_aec_aac_t*)calloc(1, sizeof(hdn_aec_aac_t));
    opt->handle = s;
    AACENC_ERROR err = AACENC_OK;
    AACENC_InfoStruct info = { 0 };
    CHANNEL_MODE mode;
    int sce = 0, cpe = 0;
    do{
        if ((err = aacEncOpen(&s->handle, 0, opt->channels)) != AACENC_OK) {
            printf("[aac] Unable to open the encoder.\n");
            break;
        }
        
        int aot;
        if(opt->audio_profile == 0) {
            aot = 2; //LC-AAC
        }else {
            aot = 5; //HE-AAC
        }
        
        //set audio object type
        if ((err = aacEncoder_SetParam(s->handle, AACENC_AOT, aot)) != AACENC_OK) {
            printf("[aac] Unable to set the AOT %d: %s\n",aot, aac_get_error(err));
            break;
        }
        
        //set samplerate
        if ((err = aacEncoder_SetParam(s->handle, AACENC_SAMPLERATE, opt->sample_rate)) != AACENC_OK) {
            printf("[aac] Unable to set the sample rate %d: %s\n",opt->sample_rate, aac_get_error(err));
            break;
        }
        
        switch (opt->channels) {
            case 1: mode = MODE_1;       sce = 1; cpe = 0; break;
            case 2: mode = MODE_2;       sce = 0; cpe = 1; break;
            default:mode = MODE_1;       sce = 1; cpe = 0; break;
        }
        
        if ((err = aacEncoder_SetParam(s->handle, AACENC_CHANNELMODE, mode)) != AACENC_OK) {
            printf("[aac] Unable to set channel mode %d: %s\n", mode, aac_get_error(err));
            break;
        }
        
        if ((err = aacEncoder_SetParam(s->handle, AACENC_CHANNELORDER, 1)) != AACENC_OK) {
            printf("[aac] Unable to set wav channel order %d: %s\n", mode, aac_get_error(err));
            break;
        }
        
        if ((err = aacEncoder_SetParam(s->handle, AACENC_BITRATE, opt->audio_bitrate)) != AACENC_OK) {
            printf("[aac] Unable to set the bitrate %d: %s\n", opt->audio_bitrate, aac_get_error(err));
            break;
        }
        if (aacEncoder_SetParam(s->handle, AACENC_TRANSMUX, 0) != AACENC_OK) {
            fprintf(stderr, "Unable to set the ADTS transmux\n");
            break;
        }
        if ((err = aacEncoder_SetParam(s->handle, AACENC_SIGNALING_MODE, 2)) != AACENC_OK) {
            fprintf(stderr, "Unable to set signaling mode %d: %s\n",2, aac_get_error(err));
            break;
        }
        
        if (aacEncEncode(s->handle, NULL, NULL, NULL, NULL) != AACENC_OK) {
            fprintf(stderr, "Unable to initialize the encoder\n");
            return 1;
        }
        if (aacEncInfo(s->handle, &info) != AACENC_OK) {
            fprintf(stderr, "Unable to get the encoder info\n");
            return 1;
        }
        opt->extradata = malloc(info.confSize);
        opt->extradata_size = info.confSize;
        memcpy(opt->extradata, info.confBuf, info.confSize);
    
        //printf("AAC encoder init success.\n");
    }while (0);
    
    if( err != AACENC_OK) {
        aacEncClose(&s->handle);
    }
    
    return 0;
}


static int encode_frame_aac(nm_av_opt* opt, uint8_t* inData, uint8_t** outData, uint32_t* outDataSize) {
    hdn_aec_aac_t* s = (hdn_aec_aac_t*)opt->handle;
    
    AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
    AACENC_InArgs in_args = { 0 };
    AACENC_OutArgs out_args = { 0 };
    int in_identifier = IN_AUDIO_DATA;
    int in_size, in_elem_size;
    int out_identifier = OUT_BITSTREAM_DATA;
    int out_size, out_elem_size;
    void *in_ptr, *out_ptr;
    uint8_t outbuf[20480];
    AACENC_ERROR err;
    

    
    in_ptr = inData;
    in_size = opt->buffer_size;
    in_elem_size = 2;
    
    in_args.numInSamples = opt->frame_size;
    in_buf.numBufs = 1;
    in_buf.bufs = &in_ptr;
    in_buf.bufferIdentifiers = &in_identifier;
    in_buf.bufSizes = &in_size;
    in_buf.bufElSizes = &in_elem_size;
    
    out_ptr = outbuf;
    out_size = sizeof(outbuf);
    out_elem_size = 1;
    out_buf.numBufs = 1;
    out_buf.bufs = &out_ptr;
    out_buf.bufferIdentifiers = &out_identifier;
    out_buf.bufSizes = &out_size;
    out_buf.bufElSizes = &out_elem_size;
    
    if ((err = aacEncEncode(s->handle, &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
        printf("[aac] Unable to encode frame: %s\n", aac_get_error(err));
        return -1;
    }
    *outData = malloc(out_args.numOutBytes);
    *outDataSize = out_args.numOutBytes;
    memcpy(*outData, outbuf, out_args.numOutBytes);
    return 0;
}

static int close_codec_aac(nm_av_opt* opt) {
    hdn_aec_aac_t* s = (hdn_aec_aac_t*)opt->handle;
    if (s->handle) {
        aacEncClose(&s->handle);
    }
    freep(opt->extradata);
    freep(opt->handle);
    return 0;
}


const nm_av_codec_t aec_aac = {
    .codec_id   = 10,
    .open_codec = open_codec_aac,
    .decode_frame = NULL,
    .encode_frame = encode_frame_aac,
    .close_codec = close_codec_aac
};