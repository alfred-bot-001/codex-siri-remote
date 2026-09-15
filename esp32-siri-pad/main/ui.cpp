#include "pad.h"
#include <initializer_list>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_3inch5_lcd_port.h"
#include "esp_io_expander_tca9554.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_system.h"
#include "esp_lcd_panel_ops.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"
#include <atomic>
#include "esp_heap_caps.h"
#include "esp_attr.h"
LV_FONT_DECLARE(font_cn20);
static lv_obj_t *mic,*capsule,*arc,*stem,*base,*status,*dot,*keys[3],*apps[2],*input_button,*mic_status;
static esp_lcd_panel_handle_t panel;
static esp_io_expander_handle_t expander;
static i2c_master_dev_handle_t pmic;
static std::atomic<uint32_t> ui_ticks{0};
static std::atomic<uint32_t> ui_heartbeat_ms{0};
static RTC_NOINIT_ATTR uint32_t ui_stall_marker;
static void ui_watchdog(void*){
 for(;;){
  vTaskDelay(pdMS_TO_TICKS(1000));
  uint32_t age=pad_millis()-ui_heartbeat_ms.load();
  if(age>10000){
   ESP_LOGE("display","UI heartbeat stalled for %lu ms; releasing input and restarting",(unsigned long)age);
   ui_stall_marker=0x55495354;
   pad_cancel_voice();
   vTaskDelay(pdMS_TO_TICKS(100));
   esp_restart();
  }
 }
}
static lv_color_t blue(){return lv_color_hex(0x245bff);}
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius){
 auto *o=lv_obj_create(parent);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
 lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_radius(o,radius,0);
 lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE);return o;
}
static lv_obj_t *text(lv_obj_t *parent,const char *s,int x,int y,const lv_font_t *font){
 auto *o=lv_label_create(parent);lv_label_set_text(o,s);lv_obj_set_pos(o,x,y);lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(0x132132),0);return o;
}
static void key_event(lv_event_t *e){
 auto code=lv_event_get_code(e);uint8_t key=(uintptr_t)lv_event_get_user_data(e);
 if(code==LV_EVENT_PRESSED)pad_touch_key(key,true);
 if(code==LV_EVENT_RELEASED||code==LV_EVENT_PRESS_LOST)pad_touch_key(key,false);
}
static void shortcut_event(lv_event_t *e){
 pad_shortcut((pad_action_t)(uintptr_t)lv_event_get_user_data(e));
}
static void tick(lv_timer_t*){
 ui_ticks++;ui_heartbeat_ms=pad_millis();
 pad_status_t s;pad_status(&s);
 static bool initialized=false;
 static pad_status_t previous{};
 // Heartbeat continues even when idle; repaint only fields visible in the UI.
 if(initialized && s.usb==previous.usb && s.ble==previous.ble &&
    s.connecting==previous.connecting && s.voice==previous.voice &&
    s.last_app==previous.last_app)return;
 previous=s;initialized=true;
 lv_label_set_text(status,s.ble?"已连接":s.connecting?"连接中":"未连接");
 lv_obj_set_style_bg_color(dot,lv_color_hex(s.ble?0x27c466:0xa5aeba),0);
 lv_color_t color=s.voice?lv_color_white():blue();
 lv_obj_set_style_bg_color(mic,s.voice?blue():lv_color_hex(0xe5f1ff),0);
 lv_obj_set_style_shadow_width(mic,s.voice?12:0,0);lv_obj_set_style_shadow_color(mic,blue(),0);lv_obj_set_style_shadow_opa(mic,LV_OPA_30,0);
 for(auto *o:{capsule,stem,base})lv_obj_set_style_bg_color(o,color,0);
 lv_obj_set_style_arc_color(arc,color,LV_PART_MAIN);
 lv_obj_set_style_opa(mic,s.usb?LV_OPA_COVER:LV_OPA_50,0);
 for(auto *o:keys)lv_obj_set_style_opa(o,s.usb?LV_OPA_COVER:LV_OPA_50,0);
 for(int i=0;i<2;i++){
  bool selected=s.last_app==(i==0?PAD_ACTION_CHATGPT:PAD_ACTION_CLAUDE);
  lv_obj_set_style_bg_color(apps[i],lv_color_hex(selected?0xffffff:0xe6e3dd),0);
  lv_obj_set_style_border_width(apps[i],selected?2:0,0);
  lv_obj_set_style_border_color(apps[i],lv_color_hex(i==0?0x12a585:0xd77b5e),0);
  lv_obj_set_style_opa(apps[i],s.usb?LV_OPA_COVER:LV_OPA_50,0);
 }
 lv_obj_set_style_opa(input_button,s.usb?LV_OPA_COVER:LV_OPA_50,0);
 lv_label_set_text(mic_status,!s.usb?"连接电脑":s.voice?"正在聆听":"准备就绪");
}
void pad_ui_init(){
 if(esp_reset_reason()==ESP_RST_SW && ui_stall_marker==0x55495354)
  ESP_LOGW("display","Recovered from a stalled UI task");
 ui_stall_marker=0;
 i2c_master_bus_config_t bc{};bc.i2c_port=I2C_NUM_0;bc.sda_io_num=GPIO_NUM_8;bc.scl_io_num=GPIO_NUM_7;
 bc.clk_source=I2C_CLK_SRC_DEFAULT;bc.glitch_ignore_cnt=7;bc.flags.enable_internal_pullup=true;
 i2c_master_bus_handle_t bus;ESP_ERROR_CHECK(i2c_new_master_bus(&bc,&bus));
 // Only enable the board's 3.3 V ALDO1 rail. Preserve charger and other rails.
 i2c_device_config_t pc{};pc.dev_addr_length=I2C_ADDR_BIT_LEN_7;pc.device_address=0x34;pc.scl_speed_hz=400000;
 ESP_ERROR_CHECK(i2c_master_bus_add_device(bus,&pc,&pmic));
 uint8_t reg=0x92,v=0; // AXP2101 ALDO1 voltage: 500 mV + N * 100 mV.
 if(i2c_master_transmit_receive(pmic,&reg,1,&v,1,100)==ESP_OK){uint8_t w[]={reg,uint8_t((v&0xe0)|28)};ESP_ERROR_CHECK(i2c_master_transmit(pmic,w,2,100));}
 reg=0x90;
 if(i2c_master_transmit_receive(pmic,&reg,1,&v,1,100)==ESP_OK){uint8_t w[]={reg,uint8_t(v|1)};ESP_ERROR_CHECK(i2c_master_transmit(pmic,w,2,100));}
 ESP_ERROR_CHECK(esp_io_expander_new_i2c_tca9554(bus,ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000,&expander));
 ESP_ERROR_CHECK(esp_io_expander_set_dir(expander,IO_EXPANDER_PIN_NUM_1,IO_EXPANDER_OUTPUT));
 ESP_ERROR_CHECK(esp_io_expander_set_level(expander,IO_EXPANDER_PIN_NUM_1,0));vTaskDelay(pdMS_TO_TICKS(100));
 ESP_ERROR_CHECK(esp_io_expander_set_level(expander,IO_EXPANDER_PIN_NUM_1,1));vTaskDelay(pdMS_TO_TICKS(150));
 esp_lcd_panel_io_handle_t io;esp_lcd_touch_handle_t touch;
 esp_3inch5_display_port_init(&io,&panel,320*40*2);esp_3inch5_touch_port_init(&touch,bus,320,480,0);
 esp_3inch5_brightness_port_init();esp_3inch5_brightness_port_set(50);
 lvgl_port_cfg_t port=ESP_LVGL_PORT_INIT_CONFIG();port.task_affinity=1;ESP_ERROR_CHECK(lvgl_port_init(&port));
 lvgl_port_display_cfg_t dc{};dc.io_handle=io;dc.panel_handle=panel;dc.buffer_size=320*40;dc.double_buffer=true;dc.hres=320;dc.vres=480;dc.rotation.mirror_x=true;dc.flags.buff_spiram=true;dc.flags.sw_rotate=true;
 auto *display=lvgl_port_add_disp(&dc);assert(display);
 lvgl_port_touch_cfg_t tc{};tc.disp=display;tc.handle=touch;assert(lvgl_port_add_touch(&tc));
 lvgl_port_lock(0);
 // LVGL rotates both the software framebuffer and pointer coordinates.
 // Keep the touch controller at rotation 0 to avoid applying this twice.
 lv_disp_set_rotation(display,LV_DISP_ROT_180);
 auto *screen=lv_scr_act();lv_obj_set_style_bg_color(screen,lv_color_hex(0xf7f6f2),0);lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
 auto *card=box(screen,12,12,296,32,0xf7f6f2,0);lv_obj_add_flag(card,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_add_event_cb(card,[](lv_event_t*){pad_request_pairing();},LV_EVENT_LONG_PRESSED,nullptr);
 auto *bt=text(card,LV_SYMBOL_BLUETOOTH,0,6,&lv_font_montserrat_20);lv_obj_set_style_text_color(bt,blue(),0);
 text(card,"Apple TV",25,8,&lv_font_montserrat_14);text(card,"遥控器",96,5,&font_cn20);
 dot=box(card,213,13,7,7,0xa5aeba,4);status=text(card,"未连接",228,5,&font_cn20);
 auto *app_group=box(screen,16,56,288,62,0xe6e3dd,30);
 for(int i=0;i<2;i++){
  apps[i]=box(app_group,4+i*140,4,140,54,0xe6e3dd,27);
  lv_obj_add_flag(apps[i],LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(apps[i],shortcut_event,LV_EVENT_CLICKED,(void*)(uintptr_t)(i==0?PAD_ACTION_CHATGPT:PAD_ACTION_CLAUDE));
  box(apps[i],12,23,7,7,i==0?0x12a585:0xd77b5e,2);
  auto *label=text(apps[i],i==0?"ChatGPT":"Claude",27,17,&lv_font_montserrat_20);
  lv_obj_set_style_bg_color(apps[i],lv_color_hex(0xffffff),LV_STATE_PRESSED);
 }
 mic=box(screen,100,150,120,120,0xe5f1ff,60);lv_obj_add_flag(mic,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_add_event_cb(mic,[](lv_event_t*){pad_toggle_board_mic();},LV_EVENT_CLICKED,nullptr);
 capsule=box(mic,49,24,22,44,0x245bff,11);
 arc=lv_arc_create(mic);lv_obj_remove_style_all(arc);lv_obj_set_pos(arc,38,40);lv_obj_set_size(arc,44,44);
 lv_arc_set_bg_angles(arc,0,180);lv_arc_set_angles(arc,0,0);lv_obj_set_style_arc_width(arc,4,LV_PART_MAIN);lv_obj_set_style_arc_color(arc,blue(),LV_PART_MAIN);lv_obj_set_style_arc_rounded(arc,true,LV_PART_MAIN);lv_obj_clear_flag(arc,LV_OBJ_FLAG_CLICKABLE);
 stem=box(mic,58,82,4,14,0x245bff,2);base=box(mic,47,94,26,4,0x245bff,2);
 mic_status=text(screen,"准备就绪",0,280,&font_cn20);lv_obj_set_width(mic_status,320);lv_obj_set_style_text_align(mic_status,LV_TEXT_ALIGN_CENTER,0);
 const char *labels[]={LV_SYMBOL_LEFT,LV_SYMBOL_RIGHT,LV_SYMBOL_NEW_LINE};uint8_t codes[]={0x50,0x4f,0x28};
 for(int i=0;i<3;i++){
  keys[i]=box(screen,i==0?16:i==1?240:88,320,i==2?144:64,92,i==2?0x245bff:0xffffff,13);
  lv_obj_add_flag(keys[i],LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_bg_color(keys[i],lv_color_hex(i==2?0x1742c8:0xdce6ff),LV_STATE_PRESSED);
  lv_obj_add_event_cb(keys[i],key_event,LV_EVENT_ALL,(void*)(uintptr_t)codes[i]);
  auto *label=text(keys[i],labels[i],0,0,&lv_font_montserrat_28);lv_obj_align(label,LV_ALIGN_CENTER,i==2?-28:0,0);
  if(i==2){lv_obj_set_style_text_color(label,lv_color_white(),0);auto *t=text(keys[i],"回车",0,0,&font_cn20);lv_obj_set_style_text_color(t,lv_color_white(),0);lv_obj_align(t,LV_ALIGN_CENTER,22,0);}
 }
 input_button=box(screen,16,428,288,44,0xffffff,12);lv_obj_add_flag(input_button,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_add_event_cb(input_button,shortcut_event,LV_EVENT_CLICKED,(void*)(uintptr_t)PAD_ACTION_INPUT);
 text(input_button,"输入法",14,11,&font_cn20);auto *switch_label=text(input_button,"切换",214,11,&font_cn20);lv_obj_set_style_text_color(switch_label,blue(),0);
 lv_obj_set_style_bg_color(input_button,lv_color_hex(0xdce6ff),LV_STATE_PRESSED);
 lv_timer_create(tick,100,nullptr);lvgl_port_unlock();pad_onboard_init(bus);
 // An explicit final panel reset/redraw restored the real display in field testing.
 // Run after all board peripherals are initialized, before exposing USB inputs.
 pad_display_recover();
 ui_heartbeat_ms=pad_millis();
 assert(xTaskCreatePinnedToCore(ui_watchdog,"ui_watchdog",3072,nullptr,3,nullptr,0)==pdPASS);
}

uint8_t *pad_ui_snapshot(size_t *size){
 pad_status_t s;pad_status(&s);if(s.voice)return nullptr;
 if(!lvgl_port_lock(100))return nullptr;
 *size=lv_snapshot_buf_size_needed(lv_scr_act(),LV_IMG_CF_TRUE_COLOR);
 uint8_t *copy=(uint8_t*)malloc(*size);lv_img_dsc_t img{};
 if(copy&&lv_snapshot_take_to_buf(lv_scr_act(),LV_IMG_CF_TRUE_COLOR,&img,copy,*size)!=LV_RES_OK){free(copy);copy=nullptr;}
 lvgl_port_unlock();return copy;
}

void pad_display_diagnostics(){
 ESP_LOGI("display","heartbeat_age=%lu heap_free=%lu internal_free=%lu",
 (unsigned long)(pad_millis()-ui_heartbeat_ms.load()),
 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
 uint32_t reset=0;uint8_t reg=0x90,rails=0;
 auto a=esp_io_expander_get_level(expander,IO_EXPANDER_PIN_NUM_1,&reset);
 auto b=i2c_master_transmit_receive(pmic,&reg,1,&rails,1,100);
 ESP_LOGI("display","uptime=%lu reset_reason=%d ticks=%lu backlight_duty=%lu freq=%lu gpio6_mux=%lx lcd_reset=%lu/%d rails=%02x/%d",
 (unsigned long)pad_millis(),esp_reset_reason(),(unsigned long)ui_ticks.load(),
 (unsigned long)ledc_get_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0),(unsigned long)ledc_get_freq(LEDC_LOW_SPEED_MODE,LEDC_TIMER_1),
 (unsigned long)REG_READ(GPIO_FUNC6_OUT_SEL_CFG_REG),(unsigned long)reset,a,rails,b);
}
void pad_display_backlight(){esp_3inch5_brightness_port_set(80);pad_display_diagnostics();}
void pad_display_recover(){
 pad_cancel_voice();
 if(!lvgl_port_lock(1000)){ESP_LOGE("display","UI lock timeout");return;}
 ESP_ERROR_CHECK(esp_io_expander_set_level(expander,IO_EXPANDER_PIN_NUM_1,0));vTaskDelay(pdMS_TO_TICKS(20));
 ESP_ERROR_CHECK(esp_io_expander_set_level(expander,IO_EXPANDER_PIN_NUM_1,1));vTaskDelay(pdMS_TO_TICKS(150));
 ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
 ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel,true));ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));
 lv_obj_invalidate(lv_scr_act());lv_refr_now(nullptr);lvgl_port_unlock();pad_display_backlight();
}
