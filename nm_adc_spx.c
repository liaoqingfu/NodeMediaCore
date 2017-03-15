//
//  nm_adc_spx.c
//  NodeMediaClient
//
//  Created by Mingliang Chen on 16/1/4.
//  Copyright © 2016年 NodeMedia. All rights reserved.
//

#include "nm_common.h"
#include "speex/speex.h"



typedef struct {
    SpeexBits bits;
    void *dec_state;
    int dec_frame_size;
    int dec_buffer_size;
    spx_int16_t* output_buffer;
} hdn_adc_spx_t;


static int open_codec_spx(nm_av_opt* opt) {
    hdn_adc_spx_t* s = (hdn_adc_spx_t*)calloc(1, sizeof(hdn_adc_spx_t));
    opt->handle = s;
    speex_bits_init(&s->bits);
    s->dec_state = speex_decoder_init(&speex_wb_mode);
    speex_decoder_ctl(s->dec_state, SPEEX_GET_FRAME_SIZE, &s->dec_frame_size);
    s->dec_buffer_size = s->dec_frame_size * sizeof(spx_int16_t);
    s->output_buffer = malloc(s->dec_buffer_size);
    opt->sample_rate = 16000;
    opt->channels = 1;
    opt->buffer_size = s->dec_buffer_size ;
    opt->frame_size = s->dec_frame_size;
    return 0;
}


static int decode_frame_spx(nm_av_opt* opt, uint8_t* inData, uint32_t inDataSize, uint8_t** outData) {
    hdn_adc_spx_t* s = (hdn_adc_spx_t*)opt->handle;
    int consumed = 0;
    if (speex_bits_remaining(&s->bits) < 5 ||
        speex_bits_peek_unsigned(&s->bits, 5) == 0xF) {
        speex_bits_read_from(&s->bits, (char*)inData, inDataSize);
        consumed = inDataSize;
    }
    
    if(speex_decode_int(s->dec_state, &s->bits, s->output_buffer) == 0 ) {
        *outData = malloc(opt->buffer_size);
        memcpy(*outData, s->output_buffer, opt->buffer_size);
        return consumed;
    } else {
        return -1;
    }
    
}

static int close_codec_spx(nm_av_opt* opt) {
    hdn_adc_spx_t* s = (hdn_adc_spx_t*)opt->handle;
    speex_decoder_destroy(s->dec_state);
    speex_bits_destroy(&s->bits);
    free(s->output_buffer);
    return 0;
}


const nm_av_codec_t adc_spx = {
    .codec_id   = 11,
    .open_codec = open_codec_spx,
    .decode_frame = decode_frame_spx,
    .encode_frame = NULL,
    .close_codec = close_codec_spx
};