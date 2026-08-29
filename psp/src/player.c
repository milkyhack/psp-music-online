#include "player.h"
#include "flac_dec.h"
#include "metrics.h"

#include <pspkernel.h>
#include <pspaudio.h>
#include <pspmp3.h>
#include <psputility.h>
#include <pspiofilemgr.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MP3_BUFFER_SIZE (128 * 1024)
#define PCM_BUFFER_SIZE (1152 * 4 * 4)
/* FLAC decode staging (max FLAC block) vs audio HW chunk (must match sceAudioChReserve). */
#define FLAC_PCM_CAP 4096
#define FLAC_OUT_SAMPLES 2048 /* multiple of 64; ~46ms @ 44.1k — headroom vs MS IO */

static volatile int g_playing = 0;
static volatile int g_paused = 0;
static volatile int g_stop_req = 0;
static volatile int g_finished = 0;
static volatile int g_growing = 0;

static int g_fd = -1;
static int g_handle = -1;
static int g_channel = -1;
static SceUID g_mp3_buf_id = -1;
static SceUID g_pcm_buf_id = -1;
static void *g_mp3_buf = NULL;
static void *g_pcm_buf = NULL;
static int g_modules = 0;
static SceUID g_audio_thid = -1;
static char g_path[280];
static char g_last_error[64];

static int g_sample_rate = 44100;
static int g_out_rate = 44100;
static int g_channels = 2;
static int g_bit_depth = 16;
static int g_bitrate = 0;
static int g_known_duration_ms = 0;
static int g_filesize = 0;
static volatile unsigned long long g_samples_played = 0;
static volatile int g_volume = 100;
static volatile int g_eq_preset = 0;
static PlayerCodec g_codec = PLAYER_CODEC_NONE;
static int g_lossless = 0;
static RingBuf *g_ring = NULL;
static int g_own_ring = 0;
static FlacDec *g_flac = NULL;
static volatile unsigned g_bytes_received = 0;
static BufferState g_buf_state = BUF_EMPTY;
static float g_prebuffer_sec = 5.0f;
static int g_flac_out_samples = FLAC_OUT_SAMPLES;

static int g_eq_gb = 256, g_eq_gm = 256, g_eq_gt = 256;
static int g_eq_lp_l = 0, g_eq_lp_r = 0;
static int g_eq_hp_l = 0, g_eq_hp_r = 0;

static const char *const EQ_NAMES[PLAYER_EQ_COUNT] = {
    "Flat", "Bass+", "Rock", "Pop", "Vocal"
};

static void set_err(const char *s) {
    if (!s) {
        g_last_error[0] = '\0';
        return;
    }
    strncpy(g_last_error, s, sizeof(g_last_error) - 1);
    g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static void eq_apply_preset(int preset) {
    if (preset < 0 || preset >= PLAYER_EQ_COUNT) {
        preset = 0;
    }
    g_eq_preset = preset;
    switch (preset) {
        case 1: g_eq_gb = 380; g_eq_gm = 240; g_eq_gt = 240; break;
        case 2: g_eq_gb = 340; g_eq_gm = 220; g_eq_gt = 320; break;
        case 3: g_eq_gb = 280; g_eq_gm = 300; g_eq_gt = 300; break;
        case 4: g_eq_gb = 200; g_eq_gm = 360; g_eq_gt = 280; break;
        default: g_eq_gb = g_eq_gm = g_eq_gt = 256; break;
    }
}

static int eq_clamp_s16(int v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return v;
}

static void eq_process_stereo(short *pcm, int bytes) {
    int n = bytes / 4;
    int i;
    int gb = g_eq_gb, gm = g_eq_gm, gt = g_eq_gt;
    if (gb == 256 && gm == 256 && gt == 256) {
        return;
    }
    for (i = 0; i < n; i++) {
        int l = pcm[i * 2];
        int r = pcm[i * 2 + 1];
        int lp_l, lp_r, hp_l, hp_r, mid_l, mid_r;
        g_eq_lp_l += (l - g_eq_lp_l) >> 5;
        g_eq_lp_r += (r - g_eq_lp_r) >> 5;
        lp_l = g_eq_lp_l;
        lp_r = g_eq_lp_r;
        g_eq_hp_l += ((l - g_eq_hp_l) * 7) >> 3;
        g_eq_hp_r += ((r - g_eq_hp_r) * 7) >> 3;
        hp_l = l - g_eq_hp_l;
        hp_r = r - g_eq_hp_r;
        mid_l = l - lp_l - hp_l;
        mid_r = r - lp_r - hp_r;
        pcm[i * 2] = (short)eq_clamp_s16((lp_l * gb + mid_l * gm + hp_l * gt) >> 8);
        pcm[i * 2 + 1] = (short)eq_clamp_s16((lp_r * gb + mid_r * gm + hp_r * gt) >> 8);
    }
}

/* Linear resample 48k→44.1k stereo in-place-ish into out. */
static int resample_48_to_441(const short *in, int in_frames, short *out, int out_cap) {
    /* ratio 441/480 = 147/160 */
    int out_frames = (int)((long long)in_frames * 441 / 480);
    int i;
    if (out_frames > out_cap) {
        out_frames = out_cap;
    }
    for (i = 0; i < out_frames; i++) {
        long long pos = (long long)i * 480 / 441;
        int i0 = (int)pos;
        int i1 = i0 + 1;
        int frac = (int)(((long long)i * 480) - (long long)i0 * 441);
        if (i1 >= in_frames) {
            i1 = in_frames - 1;
        }
        if (i0 >= in_frames) {
            i0 = in_frames - 1;
        }
        {
            int l0 = in[i0 * 2], r0 = in[i0 * 2 + 1];
            int l1 = in[i1 * 2], r1 = in[i1 * 2 + 1];
            out[i * 2] = (short)(l0 + ((l1 - l0) * frac) / 441);
            out[i * 2 + 1] = (short)(r0 + ((r1 - r0) * frac) / 441);
        }
    }
    return out_frames;
}

static int load_mp3_modules(void) {
    if (g_modules) {
        return 0;
    }
    sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
    sceUtilityLoadModule(PSP_MODULE_AV_MP3);
    g_modules = 1;
    return 0;
}

static int volume_psp(void) {
    int vol = g_volume;
    if (vol <= 0) return 0;
    if (vol >= 100) return PSP_AUDIO_VOLUME_MAX;
    return (vol * PSP_AUDIO_VOLUME_MAX) / 100;
}

static void release_resources(void) {
    if (g_channel >= 0) {
        sceAudioChRelease(g_channel);
        g_channel = -1;
    }
    if (g_handle >= 0) {
        sceMp3ReleaseMp3Handle(g_handle);
        g_handle = -1;
    }
    if (g_flac) {
        flac_dec_destroy(g_flac);
        g_flac = NULL;
    }
    if (g_fd >= 0) {
        sceIoClose(g_fd);
        g_fd = -1;
    }
    if (g_mp3_buf_id >= 0) {
        sceKernelFreePartitionMemory(g_mp3_buf_id);
        g_mp3_buf_id = -1;
        g_mp3_buf = NULL;
    }
    if (g_pcm_buf_id >= 0) {
        sceKernelFreePartitionMemory(g_pcm_buf_id);
        g_pcm_buf_id = -1;
        g_pcm_buf = NULL;
    }
    if (g_own_ring && g_ring) {
        ringbuf_destroy(g_ring);
    }
    g_ring = NULL;
    g_own_ring = 0;
    if (g_modules) {
        sceMp3TermResource();
    }
    g_codec = PLAYER_CODEC_NONE;
}

static int fill_stream_file(void) {
    unsigned char *dst = NULL;
    SceInt32 towrite = 0;
    SceInt32 pos = 0;
    int ret = sceMp3GetInfoToAddStreamData(g_handle, &dst, &towrite, &pos);
    if (ret < 0) return ret;
    if (towrite <= 0) return 0;

    for (;;) {
        int n;
        if (g_stop_req) return -1;
        if (g_growing && g_path[0]) {
            if (g_fd >= 0) {
                sceIoClose(g_fd);
                g_fd = -1;
            }
            g_fd = sceIoOpen(g_path, PSP_O_RDONLY, 0777);
            if (g_fd < 0) {
                sceKernelDelayThread(30000);
                continue;
            }
        }
        if (g_fd < 0) return -1;
        sceIoLseek32(g_fd, pos, PSP_SEEK_SET);
        n = sceIoRead(g_fd, dst, towrite);
        if (n > 0) {
            return sceMp3NotifyAddStreamData(g_handle, n);
        }
        if (!g_growing) return -1;
        sceKernelDelayThread(20000);
    }
}

static int fill_stream_ring(void) {
    unsigned char *dst = NULL;
    SceInt32 towrite = 0;
    SceInt32 pos = 0;
    size_t n;
    int ret = sceMp3GetInfoToAddStreamData(g_handle, &dst, &towrite, &pos);
    (void)pos;
    if (ret < 0) return ret;
    if (towrite <= 0) return 0;
    if (!g_ring) return -1;

    n = ringbuf_read_wait(g_ring, dst, (size_t)towrite, 80000);
    if (n > 0) {
        return sceMp3NotifyAddStreamData(g_handle, (SceInt32)n);
    }
    if (ringbuf_eof(g_ring)) {
        return -1;
    }
    return 0;
}

static void update_buffer_state(void) {
    float sec = 0.0f;
    int br = g_bitrate > 0 ? g_bitrate : (g_lossless ? 900 : 128);
    if (g_ring) {
        sec = ringbuf_buffered_seconds(g_ring, br);
        metrics_set_buffer(
            (unsigned)ringbuf_capacity(g_ring),
            (unsigned)ringbuf_used(g_ring),
            sec
        );
    }
    if (!g_playing) {
        if (sec <= 0.1f) g_buf_state = BUF_EMPTY;
        else if (sec < g_prebuffer_sec) g_buf_state = BUF_FILLING;
        else g_buf_state = BUF_READY;
    } else if (g_paused) {
        /* keep */
    } else if (sec < 0.35f) {
        g_buf_state = BUF_CRITICAL;
        metrics_note_underrun();
    } else if (sec < 2.0f) {
        g_buf_state = BUF_LOW;
    } else {
        g_buf_state = BUF_PLAYING;
    }
    metrics_set_state(g_buf_state);
}

static int audio_thread_mp3(void) {
    while (!g_stop_req && g_playing) {
        if (g_paused) {
            sceKernelDelayThread(10000);
            continue;
        }
        if (g_handle < 0 || g_channel < 0) break;

        if (sceMp3CheckStreamDataNeeded(g_handle)) {
            int fr;
            if (g_ring) {
                fr = fill_stream_ring();
            } else {
                fr = fill_stream_file();
            }
            if (fr < 0 && !g_growing && !(g_ring && !ringbuf_eof(g_ring))) {
                /* may still decode remaining */
            }
        }
        {
            short *out = NULL;
            int bytes = sceMp3Decode(g_handle, &out);
            if (bytes < 0 || bytes == 0) {
                if ((g_growing || (g_ring && !ringbuf_eof(g_ring))) && !g_stop_req) {
                    g_buf_state = BUF_REBUFFERING;
                    metrics_set_state(g_buf_state);
                    metrics_note_rebuffer();
                    sceKernelDelayThread(20000);
                    continue;
                }
                g_finished = 1;
                g_playing = 0;
                break;
            }
            {
                int frame_bytes = 2 * (g_channels > 0 ? g_channels : 2);
                if (frame_bytes > 0) {
                    g_samples_played += (unsigned long long)(bytes / frame_bytes);
                }
            }
            if (out && bytes > 0 && g_channels >= 2) {
                eq_process_stereo(out, bytes);
            }
            update_buffer_state();
            sceAudioOutputBlocking(g_channel, volume_psp(), out);
        }
    }
    return 0;
}

static int audio_thread_flac(void) {
    short pcm[FLAC_PCM_CAP * 2];
    short outbuf[FLAC_OUT_SAMPLES * 2];
    unsigned underrun_streak = 0;
    int want = g_flac_out_samples > 0 ? g_flac_out_samples : FLAC_OUT_SAMPLES;

    while (!g_stop_req && g_playing) {
        int frames = 0;
        int rc;
        short *play;
        int play_frames;
        int bytes;
        int need_in;

        if (g_paused) {
            sceKernelDelayThread(10000);
            continue;
        }
        if (!g_flac || g_channel < 0) break;

        update_buffer_state();
        /*
         * Old logic: on CRITICAL wait until 2s buffered and skip decode → music
         * froze forever at ~1s on slow Wi‑Fi (UI showed "buf 1s"). Always try to
         * decode when any compressed data exists; only silence-pad when empty.
         */
        if (g_ring && g_buf_state == BUF_CRITICAL &&
            ringbuf_used(g_ring) < 4096 && !ringbuf_eof(g_ring)) {
            short silence[FLAC_OUT_SAMPLES * 2];
            g_buf_state = BUF_REBUFFERING;
            metrics_set_state(g_buf_state);
            metrics_note_rebuffer();
            memset(silence, 0, sizeof(silence));
            sceAudioOutputBlocking(g_channel, volume_psp(), silence);
            continue;
        }

        /* Request exactly enough source frames so output matches the audio channel. */
        need_in = want;
        if (g_sample_rate == 48000) {
            need_in = (want * 480 + 440) / 441;
        } else if (g_sample_rate == 96000) {
            need_in = ((want * 480 + 440) / 441) * 2;
        }
        if (need_in > FLAC_PCM_CAP) {
            need_in = FLAC_PCM_CAP;
        }

        rc = flac_dec_read_s16(g_flac, pcm, need_in, &frames);
        if (rc < 0) {
            set_err("FLAC ERROR");
            g_finished = 1;
            g_playing = 0;
            break;
        }
        if (rc == 0 || frames <= 0) {
            if (flac_dec_eof(g_flac)) {
                g_finished = 1;
                g_playing = 0;
                break;
            }
            underrun_streak++;
            /* Keep DAC clock with silence instead of spinning without output. */
            {
                short silence[FLAC_OUT_SAMPLES * 2];
                memset(silence, 0, sizeof(silence));
                sceAudioOutputBlocking(g_channel, volume_psp(), silence);
            }
            if (underrun_streak > 400) { /* ~8s of silence @ 2048/44.1 */
                set_err("buffer underrun");
                g_finished = 1;
                g_playing = 0;
                break;
            }
            if (underrun_streak > 200 && g_sample_rate >= 96000) {
                set_err("Unsupported: 96 kHz");
                g_finished = 1;
                g_playing = 0;
                break;
            }
            continue;
        }
        underrun_streak = 0;

        /* mono → stereo */
        if (g_channels == 1) {
            int i;
            for (i = frames - 1; i >= 0; i--) {
                short s = pcm[i];
                pcm[i * 2] = s;
                pcm[i * 2 + 1] = s;
            }
        }

        play = pcm;
        play_frames = frames;
        if (g_sample_rate == 48000) {
            play_frames = resample_48_to_441(pcm, frames, outbuf, want);
            play = outbuf;
        } else if (g_sample_rate == 96000) {
            /* crude 96→44.1: first 96→48 then 48→44.1 */
            short mid[FLAC_PCM_CAP * 2];
            int mid_frames = frames / 2;
            int i;
            for (i = 0; i < mid_frames; i++) {
                mid[i * 2] = pcm[i * 4];
                mid[i * 2 + 1] = pcm[i * 4 + 1];
            }
            play_frames = resample_48_to_441(mid, mid_frames, outbuf, want);
            play = outbuf;
        }

        /* Pad/trim to exact HW buffer — sceAudioOutputBlocking always drains want samples. */
        if (play_frames < want) {
            if (play != outbuf) {
                memcpy(outbuf, play, (size_t)play_frames * 4u);
                play = outbuf;
            }
            memset(outbuf + play_frames * 2, 0, (size_t)(want - play_frames) * 4u);
            play_frames = want;
        } else if (play_frames > want) {
            play_frames = want;
        }

        bytes = play_frames * 4;
        if (play && bytes > 0) {
            eq_process_stereo(play, bytes);
        }
        g_samples_played += (unsigned long long)play_frames;
        {
            /* consumption ~ 44100*16*2 */
            metrics_set_consumption((44100.0f * 16.0f * 2.0f) / 1000000.0f);
            metrics_set_decode_rate(metrics_get()->flacDecodeRateMbps);
        }
        sceAudioOutputBlocking(g_channel, volume_psp(), play);
    }
    return 0;
}

static int audio_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    if (g_codec == PLAYER_CODEC_FLAC) {
        return audio_thread_flac();
    }
    return audio_thread_mp3();
}

static void join_audio_thread(void) {
    if (g_audio_thid < 0) return;
    g_stop_req = 1;
    {
        SceUInt timeout = 500000;
        int wr = sceKernelWaitThreadEnd(g_audio_thid, &timeout);
        if (wr < 0) {
            sceKernelTerminateDeleteThread(g_audio_thid);
        } else {
            sceKernelDeleteThread(g_audio_thid);
        }
    }
    g_audio_thid = -1;
}

void player_stop(void) {
    g_stop_req = 1;
    g_paused = 0;
    g_growing = 0;
    if (g_ring) {
        ringbuf_set_abort(g_ring, 1);
    }
    join_audio_thread();
    g_playing = 0;
    g_finished = 0;
    g_samples_played = 0;
    g_known_duration_ms = 0;
    g_bitrate = 0;
    g_bytes_received = 0;
    g_buf_state = BUF_EMPTY;
    release_resources();
    g_stop_req = 0;
    set_err(NULL);
}

void player_set_growing(int growing) {
    g_growing = growing ? 1 : 0;
}

int player_is_playing(void) { return g_playing && !g_paused; }
int player_is_paused(void) { return g_playing && g_paused; }
int player_is_active(void) { return g_playing; }

void player_pause(void) { if (g_playing) g_paused = 1; }
void player_resume(void) { if (g_playing) g_paused = 0; }
void player_toggle_pause(void) {
    if (!g_playing) return;
    g_paused = !g_paused;
}

int player_elapsed_ms(void) {
    int rate = g_out_rate > 0 ? g_out_rate : g_sample_rate;
    if (rate <= 0) return 0;
    return (int)((g_samples_played * 1000ULL) / (unsigned long long)rate);
}

int player_duration_ms(void) {
    if (g_known_duration_ms > 0) return g_known_duration_ms;
    if (g_codec == PLAYER_CODEC_FLAC && g_flac) {
        const FlacInfo *fi = flac_dec_info(g_flac);
        if (fi && fi->sample_rate > 0 && fi->total_samples > 0) {
            return (int)((fi->total_samples * 1000ULL) / fi->sample_rate);
        }
    }
    if (g_bitrate <= 0 || g_filesize <= 0) return 0;
    return (int)(((long long)g_filesize * 8LL * 1000LL) / ((long long)g_bitrate * 1000LL));
}

void player_set_known_duration_ms(int ms) {
    if (ms < 0) ms = 0;
    g_known_duration_ms = ms;
    if (ms > 0 && g_filesize > 0) {
        int kbps = (int)(((long long)g_filesize * 8LL) / (long long)ms);
        if (kbps > 0 && kbps < 5000) g_bitrate = kbps;
    }
}

void player_set_elapsed_ms(int ms) {
    int rate = g_out_rate > 0 ? g_out_rate : g_sample_rate;
    if (ms < 0) {
        ms = 0;
    }
    if (rate <= 0) {
        rate = 44100;
    }
    g_samples_played = ((unsigned long long)ms * (unsigned long long)rate) / 1000ULL;
}

int player_get_volume(void) { return g_volume; }
void player_set_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    g_volume = vol;
}

int player_bitrate_kbps(void) { return g_bitrate; }
int player_sample_khz(void) { return g_sample_rate / 1000; }
int player_bit_depth(void) { return g_bit_depth; }
int player_is_stereo(void) { return g_channels >= 2; }
int player_is_lossless(void) { return g_lossless; }
PlayerCodec player_codec(void) { return g_codec; }
const char *player_format_name(void) {
    if (g_codec == PLAYER_CODEC_FLAC) return "FLAC";
    if (g_codec == PLAYER_CODEC_MP3) return "MP3";
    return "—";
}
BufferState player_buffer_state(void) { return g_buf_state; }
float player_buffered_seconds(void) {
    int br = g_bitrate > 0 ? g_bitrate : (g_lossless ? 900 : 128);
    return g_ring ? ringbuf_buffered_seconds(g_ring, br) : 0.0f;
}
RingBuf *player_ring(void) { return g_ring; }
unsigned player_bytes_received(void) { return g_bytes_received; }
void player_set_bytes_received(unsigned n) { g_bytes_received = n; }
void player_set_stream_eof(void) {
    if (g_ring) ringbuf_set_eof(g_ring, 1);
    g_growing = 0;
}
const char *player_last_error(void) {
    return g_last_error[0] ? g_last_error : "";
}

static int start_audio_thread(void) {
    g_playing = 1;
    g_audio_thid = sceKernelCreateThread("audio", audio_thread, 0x14, 0x10000, PSP_THREAD_ATTR_USER, NULL);
    if (g_audio_thid < 0) {
        player_stop();
        return -8;
    }
    if (sceKernelStartThread(g_audio_thid, 0, NULL) < 0) {
        sceKernelDeleteThread(g_audio_thid);
        g_audio_thid = -1;
        player_stop();
        return -9;
    }
    return 0;
}

static int detect_flac_path(const char *path) {
    const char *dot;
    char magic[4];
    int fd;
    if (!path) return 0;
    dot = strrchr(path, '.');
    if (dot && (strcmp(dot, ".flac") == 0 || strcmp(dot, ".FLAC") == 0)) {
        return 1;
    }
    fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) return 0;
    if (sceIoRead(fd, magic, 4) == 4 && memcmp(magic, "fLaC", 4) == 0) {
        sceIoClose(fd);
        return 1;
    }
    sceIoClose(fd);
    return 0;
}

static int player_start_mp3_common(int stream_end, int growing, RingBuf *rb) {
    int samples;
    player_stop();
    load_mp3_modules();
    g_codec = PLAYER_CODEC_MP3;
    g_lossless = 0;
    g_bit_depth = 16;
    g_ring = rb;
    g_own_ring = 0;

    if (!rb) {
        g_fd = sceIoOpen(g_path, PSP_O_RDONLY, 0777);
        if (g_fd < 0) {
            set_err("open");
            return -1;
        }
        if (stream_end > 0) {
            g_filesize = stream_end;
        } else {
            g_filesize = sceIoLseek32(g_fd, 0, PSP_SEEK_END);
            sceIoLseek32(g_fd, 0, PSP_SEEK_SET);
        }
    } else {
        g_filesize = stream_end > 0 ? stream_end : 0;
    }

    g_mp3_buf_id = sceKernelAllocPartitionMemory(
        PSP_MEMORY_PARTITION_USER, "mp3buf", PSP_SMEM_Low, MP3_BUFFER_SIZE, NULL
    );
    g_pcm_buf_id = sceKernelAllocPartitionMemory(
        PSP_MEMORY_PARTITION_USER, "pcmbuff", PSP_SMEM_Low, PCM_BUFFER_SIZE, NULL
    );
    if (g_mp3_buf_id < 0 || g_pcm_buf_id < 0) {
        set_err("insufficient RAM");
        player_stop();
        return -2;
    }
    g_mp3_buf = sceKernelGetBlockHeadAddr(g_mp3_buf_id);
    g_pcm_buf = sceKernelGetBlockHeadAddr(g_pcm_buf_id);

    if (sceMp3InitResource() < 0) {
        player_stop();
        return -3;
    }
    {
        SceMp3InitArg arg;
        memset(&arg, 0, sizeof(arg));
        arg.mp3StreamStart = 0;
        arg.mp3StreamEnd = g_filesize > 0 ? g_filesize : 0x7FFFFFFF;
        arg.mp3Buf = g_mp3_buf;
        arg.mp3BufSize = MP3_BUFFER_SIZE;
        arg.pcmBuf = g_pcm_buf;
        arg.pcmBufSize = PCM_BUFFER_SIZE;
        g_handle = sceMp3ReserveMp3Handle(&arg);
        if (g_handle < 0) {
            player_stop();
            return -4;
        }
    }
    g_growing = growing ? 1 : 0;
    if (g_ring) {
        if (fill_stream_ring() < 0 && ringbuf_used(g_ring) == 0) {
            /* wait a bit for first data */
            int t;
            for (t = 0; t < 50 && ringbuf_used(g_ring) < 2048 && !ringbuf_eof(g_ring); t++) {
                sceKernelDelayThread(20000);
            }
            if (fill_stream_ring() < 0 && ringbuf_used(g_ring) == 0) {
                set_err("insufficient buffer");
                player_stop();
                return -5;
            }
        }
    } else if (fill_stream_file() < 0) {
        player_stop();
        return -5;
    }
    if (sceMp3Init(g_handle) < 0) {
        set_err("decoder error");
        player_stop();
        return -6;
    }
    sceMp3SetLoopNum(g_handle, 0);
    g_sample_rate = sceMp3GetSamplingRate(g_handle);
    g_channels = sceMp3GetMp3ChannelNum(g_handle);
    if (g_sample_rate <= 0) g_sample_rate = 44100;
    if (g_channels <= 0) g_channels = 2;
    g_out_rate = g_sample_rate;
    g_samples_played = 0;
    g_paused = 0;
    g_finished = 0;
    g_stop_req = 0;
    samples = sceMp3GetMaxOutputSample(g_handle);
    g_channel = sceAudioChReserve(-1, samples, PSP_AUDIO_FORMAT_STEREO);
    if (g_channel < 0) {
        player_stop();
        return -7;
    }
    metrics_set_format("MP3", 0, g_sample_rate, 16, g_channels);
    metrics_set_consumption((float)(g_sample_rate * 16 * g_channels) / 1000000.0f);
    return start_audio_thread();
}

static int player_start_flac(RingBuf *rb, const char *path) {
    char ferr[64];
    const FlacInfo *fi;
    int samples = FLAC_OUT_SAMPLES;

    player_stop();
    g_codec = PLAYER_CODEC_FLAC;
    g_lossless = 1;
    g_ring = rb;
    g_own_ring = 0;
    g_flac_out_samples = samples;

    g_flac = flac_dec_create();
    if (!g_flac) {
        set_err("insufficient RAM");
        return -2;
    }
    if (rb) {
        /* wait for STREAMINFO bytes — main already waited for play-ready buffer */
        {
            int t;
            size_t need = 16 * 1024;
            for (t = 0; t < 100; t++) {
                if (ringbuf_used(rb) >= need || ringbuf_eof(rb)) break;
                g_buf_state = BUF_FILLING;
                metrics_set_state(g_buf_state);
                sceKernelDelayThread(15000);
            }
            if (ringbuf_used(rb) < 4096) {
                set_err("insufficient buffer");
                player_stop();
                return -5;
            }
        }
        if (flac_dec_open_ring(g_flac, rb) < 0) {
            set_err(flac_dec_error_str(g_flac));
            player_stop();
            return -6;
        }
    } else {
        if (flac_dec_open_file(g_flac, path) < 0) {
            set_err(flac_dec_error_str(g_flac));
            player_stop();
            return -1;
        }
    }

    fi = flac_dec_info(g_flac);
    if (!fi || !flac_format_supported(fi, ferr, sizeof(ferr))) {
        set_err(ferr[0] ? ferr : "unsupported");
        player_stop();
        return -10;
    }
    g_sample_rate = (int)fi->sample_rate;
    g_channels = (int)fi->channels;
    g_bit_depth = (int)fi->bits_per_sample;
    g_out_rate = 44100;
    if (g_sample_rate == 44100) {
        g_out_rate = 44100;
    }
    /* bitrate estimate for buffer seconds */
    g_bitrate = (int)((g_sample_rate * g_bit_depth * g_channels) / 1000 * 0.6f);
    if (g_bitrate < 200) g_bitrate = 700;

    /* Short extra wait only if still critically low (main already prebuffered). */
    if (rb) {
        int t;
        for (t = 0; t < 120; t++) {
            float sec = ringbuf_buffered_seconds(rb, g_bitrate);
            metrics_set_buffer((unsigned)ringbuf_capacity(rb), (unsigned)ringbuf_used(rb), sec);
            if (sec >= g_prebuffer_sec || ringbuf_eof(rb) || ringbuf_used(rb) * 100 >= ringbuf_capacity(rb) * 80) {
                g_buf_state = BUF_READY;
                metrics_set_state(g_buf_state);
                break;
            }
            g_buf_state = BUF_FILLING;
            metrics_set_state(g_buf_state);
            sceKernelDelayThread(15000);
        }
        if (ringbuf_buffered_seconds(rb, g_bitrate) < 1.5f && !ringbuf_eof(rb) &&
            ringbuf_used(rb) < 48 * 1024) {
            set_err("insufficient buffer");
            player_stop();
            return -5;
        }
    }

    g_samples_played = 0;
    g_paused = 0;
    g_finished = 0;
    g_stop_req = 0;
    /* Higher priority than UI/download so local MS reads don't starve the DAC. */
    g_channel = sceAudioChReserve(-1, samples, PSP_AUDIO_FORMAT_STEREO);
    if (g_channel < 0) {
        player_stop();
        return -7;
    }
    metrics_set_format("FLAC", 1, g_sample_rate, g_bit_depth, g_channels);
    metrics_set_consumption((44100.0f * 16.0f * 2.0f) / 1000000.0f);
    {
        int rc;
        g_playing = 1;
        g_audio_thid = sceKernelCreateThread(
            "audio",
            audio_thread,
            0x12,
            0x18000,
            PSP_THREAD_ATTR_USER,
            NULL
        );
        if (g_audio_thid < 0) {
            player_stop();
            return -8;
        }
        rc = sceKernelStartThread(g_audio_thid, 0, NULL);
        if (rc < 0) {
            sceKernelDeleteThread(g_audio_thid);
            g_audio_thid = -1;
            player_stop();
            return -9;
        }
    }
    return 0;
}

int player_play_file(const char *path) {
    memset(g_path, 0, sizeof(g_path));
    if (path) strncpy(g_path, path, sizeof(g_path) - 1);
    if (detect_flac_path(path)) {
        return player_start_flac(NULL, path);
    }
    return player_start_mp3_common(0, 0, NULL);
}

int player_play_growing(const char *path, int expected_size) {
    memset(g_path, 0, sizeof(g_path));
    if (path) strncpy(g_path, path, sizeof(g_path) - 1);
    if (expected_size <= 0) {
        return player_play_file(path);
    }
    return player_start_mp3_common(expected_size, 1, NULL);
}

int player_play_ring(RingBuf *rb, PlayerCodec codec, int expected_bytes) {
    if (!rb) return -1;
    g_filesize = expected_bytes > 0 ? expected_bytes : 0;
    if (codec == PLAYER_CODEC_FLAC) {
        return player_start_flac(rb, NULL);
    }
    memset(g_path, 0, sizeof(g_path));
    return player_start_mp3_common(expected_bytes > 0 ? expected_bytes : 0x7FFFFFFF, 1, rb);
}

int player_update(void) {
    if (g_finished) {
        if (g_audio_thid >= 0) {
            join_audio_thread();
            release_resources();
            g_stop_req = 0;
        }
        g_finished = 0;
        g_growing = 0;
        return 0;
    }
    update_buffer_state();
    return 1;
}

void player_set_eq_preset(int preset) {
    eq_apply_preset(preset);
    g_eq_lp_l = g_eq_lp_r = 0;
    g_eq_hp_l = g_eq_hp_r = 0;
}

int player_get_eq_preset(void) { return g_eq_preset; }

const char *player_eq_preset_name(int preset) {
    if (preset < 0 || preset >= PLAYER_EQ_COUNT) preset = 0;
    return EQ_NAMES[preset];
}
