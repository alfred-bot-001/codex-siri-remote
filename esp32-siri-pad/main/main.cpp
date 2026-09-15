#include "pad.h"
#include "nvs_flash.h"
extern "C" void app_main(){
 ESP_ERROR_CHECK(nvs_flash_init()); // Preserve the paired remote; never erase NVS automatically.
 pad_init();pad_ui_init();pad_usb_init();pad_ble_init();
}
