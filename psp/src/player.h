#ifndef PLAYER_H
#define PLAYER_H

#include "ringbuf.h"
#include "metrics.h"

typedef enum {
    PLAYER_CODEC_NONE = 0,
    PLAYER_CODEC_MP3,
    PLAYER_CODEC_FLAC
} PlayerCodec;

/* Local complete file (MP3 or FLAC by extension / magic). */
int player_play_file(const char *path);

/* Growing MP3 file on MS — legacy; prefer ring for online. */
int player_play_growing(const char *path, int expected_size);

/* Online: decode from RAM ring. Codec from content-type / path hint. */
int player_play_ring(RingBuf *rb, PlayerCodec codec, int expected_bytes);

void player_set_growing(int growing);
void player_stop(void);
int player_is_playing(void);
int player_is_paused(void);
int player_is_active(void);
void player_pause(void);
void player_resume(void);
void player_toggle_pause(void);
int player_update(void);

int player_elapsed_ms(void);
int player_duration_ms(void);
void player_set_known_duration_ms(int ms);
void player_set_elapsed_ms(int ms);
int player_get_volume(void);
void player_set_volume(int vol);
int player_bitrate_kbps(void);
int player_sample_khz(void);
int player_bit_depth(void);
int player_is_stereo(void);
int player_is_lossless(void);
PlayerCodec player_codec(void);
const char *player_format_name(void);
BufferState player_buffer_state(void);
float player_buffered_seconds(void);

RingBuf *player_ring(void);
unsigned player_bytes_received(void);
void player_set_bytes_received(unsigned n);
void player_set_stream_eof(void);

#define PLAYER_EQ_COUNT 5
void player_set_eq_preset(int preset);
int player_get_eq_preset(void);
const char *player_eq_preset_name(int preset);

const char *player_last_error(void);

#endif
