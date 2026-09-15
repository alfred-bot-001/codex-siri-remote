#include "pad.h"
#include <NimBLEDevice.h>
#include <atomic>
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
static const char *TAG="remote";
static NimBLEClient *client=nullptr;
static NimBLEAddress target;
static bool have_target=false;
static std::atomic<bool> found{false},disconnected{false};
static std::atomic<uint16_t> audio_handle{0},button_handle{0};
static const char *identity="28:2d:7f:3f:50:6e";
class Events:public NimBLEClientCallbacks {
 void onDisconnect(NimBLEClient*,int reason)override{pad_remote_connected(false);disconnected=true;ESP_LOGI(TAG,"Disconnected %d",reason);}
} events;
class ScanEvents:public NimBLEScanCallbacks {
 void onResult(const NimBLEAdvertisedDevice *dev)override{
  if(found||dev->getRSSI()<-60||!dev->isAdvertisingService(NimBLEUUID(uint16_t(0x1812))))return;
  auto m=dev->getManufacturerData();
  if(m.size()<14||uint8_t(m[0])!=0x4c||m[1]!=0||m[2]!=7||m[3]!=13)return;
  char id[18];snprintf(id,sizeof(id),"%02x:%02x:%02x:%02x:%02x:%02x",uint8_t(m[8]),uint8_t(m[9]),uint8_t(m[10]),uint8_t(m[11]),uint8_t(m[12]),uint8_t(m[13]));
  if(strcmp(id,identity))return;
  target=dev->getAddress();NimBLEDevice::getScan()->stop();found=true;
 }
} scans;
static void notify(NimBLERemoteCharacteristic *ch,uint8_t *data,size_t n,bool){
 if(ch->getHandle()==button_handle&&n>=2)pad_remote_buttons(data[0]|(data[1]<<8));
 else if(ch->getHandle()==audio_handle)pad_remote_audio(data,n);
}
static bool connect_remote(){
 pad_remote_connected(false);pad_remote_connecting(true);audio_handle=button_handle=0;
 if(client){NimBLEDevice::deleteClient(client);client=nullptr;}
 client=NimBLEDevice::createClient();client->setClientCallbacks(&events,false);
 client->setConnectTimeout(10000);client->setConnectionParams(12,12,0,200);
 if(!client->connect(target)||!client->secureConnection())return false;
 auto *hid=client->getService(NimBLEUUID(uint16_t(0x1812)));if(!hid)return false;
 auto chars=hid->getCharacteristics(true);NimBLERemoteCharacteristic *a=nullptr,*b=nullptr,*enable=nullptr;
 for(auto *ch:chars){
  if(ch->getUUID()!=NimBLEUUID(uint16_t(0x2a4d)))continue;
  auto *ref=ch->getDescriptor(NimBLEUUID(uint16_t(0x2908)));if(!ref)continue;
  auto v=ref->readValue();if(v.size()<2)continue;
  if(uint8_t(v[0])==0xfa&&v[1]==1)a=ch;
  if(uint8_t(v[0])==0xfb&&v[1]==1)b=ch;
  if(uint8_t(v[0])==0xf0&&(v[1]==2||v[1]==3))enable=ch;
 }
 if(!a||!b||!enable)return false;
 audio_handle=a->getHandle();button_handle=b->getHandle();
 if(!a->subscribe(true,notify)||!b->subscribe(true,notify))return false;
 uint8_t value=0xaf;if(!enable->writeValue(&value,1,!enable->canWriteNoResponse()))return false;
 pad_remote_connected(true);ESP_LOGI(TAG,"Ready interval=%u",client->getConnInfo().getConnInterval());return true;
}
static void ble_task(void*){
 NimBLEDevice::init("Siri Voice Pad");NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);NimBLEDevice::setSecurityAuth(true,false,true);NimBLEDevice::setMTU(185);
 auto *scan=NimBLEDevice::getScan();scan->setScanCallbacks(&scans,false);scan->setActiveScan(true);scan->setInterval(100);scan->setWindow(90);
 ESP_LOGI(TAG,"Bonds=%d",NimBLEDevice::getNumBonds());
 for(int i=0;i<NimBLEDevice::getNumBonds();i++){auto addr=NimBLEDevice::getBondedAddress(i);ESP_LOGI(TAG,"Bond=%s",addr.toString().c_str());if(strcasecmp(addr.toString().c_str(),identity)==0){target=addr;have_target=true;}}
 // The exact remote was physically verified in the previous test firmware.
 if(!have_target){target=NimBLEAddress(identity,0);have_target=true;}
 uint32_t next=pad_millis()+1000;unsigned attempt=0;
 for(;;){
  if(pad_take_pairing_request()){
   if(client&&client->isConnected())client->disconnect();
   have_target=false;pad_remote_connecting(true);scan->start(120000,false,true);
  }
  if(found.exchange(false)){have_target=true;next=pad_millis();attempt=0;}
  if(disconnected.exchange(false))next=pad_millis()+2000;
  if(have_target&&(!client||!client->isConnected())&&int32_t(pad_millis()-next)>=0){
   scan->stop();if(connect_remote()){attempt=0;}else{if(client&&client->isConnected())client->disconnect();attempt++;}
   pad_remote_connecting(false);next=pad_millis()+(attempt<5?3000:15000);
  }
  vTaskDelay(pdMS_TO_TICKS(50));
 }
}
void pad_ble_init(){xTaskCreatePinnedToCore(ble_task,"remote",10240,nullptr,4,nullptr,0);}
