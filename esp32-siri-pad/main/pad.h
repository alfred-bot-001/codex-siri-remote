#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { MIC_OFF, MIC_REMOTE, MIC_BOARD } mic_source_t;
typedef enum { PAD_ACTION_NONE, PAD_ACTION_CHATGPT, PAD_ACTION_CLAUDE, PAD_ACTION_INPUT } pad_action_t;
typedef struct {
    bool usb, ble, voice, board_ready, connecting;
    mic_source_t source;
    pad_action_t last_app; // Last local command, not computer application state.
    uint32_t audio_frames, audio_errors, lost_packets, fifo_drops, last_peak;
} pad_status_t;
void pad_init(void);
void pad_remote_connected(bool ready);
void pad_remote_connecting(bool active);
uint8_t *pad_ui_snapshot(size_t *size);
void pad_remote_buttons(uint16_t mask);
void pad_remote_audio(const uint8_t *data, size_t len);
void pad_usb_connected(bool ready);
void pad_touch_key(uint8_t key, bool down);
void pad_shortcut(pad_action_t action);
void pad_toggle_board_mic(void);
void pad_request_pairing(void);
bool pad_take_pairing_request(void);
void pad_cancel_voice(void);
void pad_set_board_ready(bool ready);
bool pad_board_active(void);
uint32_t pad_board_token(void);
void pad_board_pcm(const int16_t *samples, size_t count, uint32_t token);
void pad_read_pcm(int16_t *samples, size_t count);
void pad_status(pad_status_t *out);
bool pad_next_hid(uint8_t report[8]);
void pad_service(void);
void pad_usb_init(void);
void pad_ble_init(void);
void pad_ui_init(void);
void pad_display_diagnostics(void);
void pad_display_backlight(void);
void pad_display_recover(void);
void pad_onboard_init(void *bus);
uint32_t pad_millis(void);
#ifdef __cplusplus
}
#endif
