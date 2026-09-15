#pragma once
#include <cstdint>
#define ESP_OPUS_DEC_FRAME_DURATION_20_MS 3
#define ESP_AUDIO_ERR_OK 0
struct esp_opus_dec_cfg_t {unsigned sample_rate;uint8_t channel;int duration;bool self_delimited;};
struct esp_audio_dec_in_raw_t {uint8_t *buffer;unsigned len;};
struct esp_audio_dec_out_frame_t {uint8_t *buffer;unsigned len,decoded_size;};
struct esp_audio_dec_info_t {unsigned sample_rate;uint8_t channel;};
inline int esp_opus_dec_open(void*,unsigned,void**){return -1;}
inline int esp_opus_dec_decode(void*,void*,void*,void*){return -1;}
inline int esp_opus_dec_close(void*){return 0;}
