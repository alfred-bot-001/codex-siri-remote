#include "pad.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
static esp_codec_dev_handle_t input;
static void capture(void*){
 int16_t samples[480];bool opened=false;uint32_t opened_epoch=0;
 esp_codec_dev_sample_info_t fs{};fs.sample_rate=48000;fs.channel=1;fs.bits_per_sample=16;
 for(;;){
  uint32_t token=pad_board_token();
  if(opened&&token!=opened_epoch){esp_codec_dev_close(input);opened=false;}
  if(!token){
   if(opened){esp_codec_dev_close(input);opened=false;}
   vTaskDelay(pdMS_TO_TICKS(10));continue;
  }
  if(!opened){
   if(esp_codec_dev_open(input,&fs)!=ESP_CODEC_DEV_OK){pad_set_board_ready(false);continue;}
   esp_codec_dev_set_in_gain(input,30.0);opened=true;opened_epoch=token;
   // Discard initial codec settling data rather than sending it to the host.
   for(int i=0;i<4&&pad_board_active();i++)esp_codec_dev_read(input,samples,sizeof(samples));
  }
  int err=esp_codec_dev_read(input,samples,sizeof(samples));
  if(err==ESP_CODEC_DEV_OK)pad_board_pcm(samples,480,token);
  else{ESP_LOGE("onboard","read failed %d",err);pad_set_board_ready(false);}
 }
}
void pad_onboard_init(void *bus){
 i2s_chan_handle_t tx=nullptr,rx=nullptr;
 i2s_chan_config_t chan=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
 i2s_std_config_t cfg{};cfg.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(48000);
 cfg.slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
 cfg.gpio_cfg.mclk=GPIO_NUM_12;cfg.gpio_cfg.bclk=GPIO_NUM_13;cfg.gpio_cfg.ws=GPIO_NUM_15;cfg.gpio_cfg.dout=GPIO_NUM_16;cfg.gpio_cfg.din=GPIO_NUM_14;
 ESP_ERROR_CHECK(i2s_new_channel(&chan,&tx,&rx));
 ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx,&cfg));ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx,&cfg));
 ESP_ERROR_CHECK(i2s_channel_enable(tx));ESP_ERROR_CHECK(i2s_channel_enable(rx));
 audio_codec_i2s_cfg_t dc{};dc.rx_handle=rx;dc.tx_handle=tx;
 auto *data=audio_codec_new_i2s_data(&dc);
 audio_codec_i2c_cfg_t cc{};cc.addr=ES8311_CODEC_DEFAULT_ADDR;cc.bus_handle=bus;
 auto *ctrl=audio_codec_new_i2c_ctrl(&cc);
 es8311_codec_cfg_t codec{};codec.codec_mode=ESP_CODEC_DEV_WORK_MODE_ADC;codec.ctrl_if=ctrl;
 codec.gpio_if=audio_codec_new_gpio();codec.pa_pin=GPIO_NUM_NC;codec.use_mclk=true;
 auto *dev=es8311_codec_new(&codec);
 if(!data||!ctrl||!dev){ESP_LOGE("onboard","codec init failed");return;}
 esp_codec_dev_cfg_t ic{};ic.dev_type=ESP_CODEC_DEV_TYPE_IN;ic.codec_if=dev;ic.data_if=data;
 input=esp_codec_dev_new(&ic);if(!input)return;
 pad_set_board_ready(true);xTaskCreatePinnedToCore(capture,"board_mic",6144,nullptr,5,nullptr,1);
}
