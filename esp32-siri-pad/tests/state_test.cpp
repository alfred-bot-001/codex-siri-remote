#include "pad.h"
#include "esp_timer.h"
#include <array>
#include <vector>
#include <cassert>
#include <cstdio>
using Report=std::array<uint8_t,8>;
static std::vector<Report> drain(){std::vector<Report> rs;Report r;while(pad_next_hid(r.data()))rs.push_back(r);return rs;}
static pad_status_t status(){pad_status_t s;pad_status(&s);return s;}
static void reset(){pad_usb_connected(false);pad_remote_connected(false);pad_usb_connected(true);pad_remote_connected(true);pad_remote_buttons(0);pad_set_board_ready(true);drain();}
int main(){
 pad_init();reset();
 // Right Option down/up must survive even a short press without audio.
 pad_remote_buttons(0x20);pad_remote_buttons(0);auto r=drain();assert(r.size()==2&&r[0][0]==0x40&&r[1]==Report{});
 // Exact HID key usages, including simultaneous keys and release.
 pad_remote_buttons(0x1408);r=drain();assert(r.size()==1&&r[0][2]==0x28&&r[0][3]==0x50&&r[0][4]==0x4f);pad_remote_buttons(0);assert(drain().back()==Report{});
 // A held button during connection must not inject input until a release.
 reset();pad_remote_connected(true);pad_remote_buttons(0x28);assert(drain().empty());assert(!status().voice);pad_remote_buttons(0);pad_remote_buttons(0x20);assert(status().source==MIC_REMOTE);
 // Disconnect releases Option and drops all buffered audio.
 pad_remote_connected(false);assert(drain().back()==Report{});assert(!status().voice);
 // Remote takes priority and must not restart a previously toggled board mic.
 reset();pad_toggle_board_mic();assert(status().source==MIC_BOARD);pad_remote_buttons(0x20);assert(status().source==MIC_REMOTE);pad_toggle_board_mic();assert(status().source==MIC_REMOTE);pad_remote_buttons(0);assert(status().source==MIC_OFF);
 // USB loss cancels toggle and held keys. Reconnect emits a neutral report.
 reset();pad_toggle_board_mic();pad_touch_key(0x28,true);pad_usb_connected(false);assert(!status().voice);pad_usb_connected(true);r=drain();assert(r.size()==1&&r[0]==Report{});
 // Stalled audio releases Option and cannot restart from another held report.
 reset();pad_remote_buttons(0x20);test_time_ms+=1001;pad_service();assert(!status().voice);pad_remote_buttons(0x28);assert(!status().voice);pad_remote_buttons(0);pad_remote_buttons(0x20);assert(status().voice);
 // Old board capture sessions may never leak into a new capture session.
 reset();pad_toggle_board_mic();auto old=pad_board_token();pad_toggle_board_mic();pad_toggle_board_mic();int16_t input[960],out[48];for(auto &v:input)v=1234;
 pad_board_pcm(input,960,old);pad_read_pcm(out,48);for(auto v:out)assert(v==0);
 pad_board_pcm(input,960,pad_board_token());pad_read_pcm(out,48);for(auto v:out)assert(v==1234);
 pad_toggle_board_mic();pad_read_pcm(out,48);for(auto v:out)assert(v==0);
 // HID backlog overflow must fail closed instead of keeping Option held.
 reset();pad_toggle_board_mic();for(int i=0;i<70;i++)pad_touch_key(0x28,i%2==0);assert(!status().voice);r=drain();assert(r.back()==Report{});
 // Malformed voice packets cannot access outside supplied data.
 reset();pad_remote_buttons(0x20);uint8_t short_frame[5]={0,0,0,0,94};pad_remote_audio(short_frame,5);pad_remote_audio(short_frame,0);assert(status().audio_errors==0);
 // A voice-end sentinel releases the modifier immediately.
 short_frame[4]=0;pad_remote_audio(short_frame,5);assert(!status().voice);assert(drain().back()==Report{});
 puts("PASS: 11 state, disconnect, stale-audio, source-isolation and HID scenarios");
}
