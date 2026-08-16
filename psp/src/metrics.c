#include "metrics.h"

#include <pspkernel.h>
#include <stdio.h>
#include <string.h>

static AudioMetrics g_m;
static unsigned g_net_tick;
static unsigned long long g_net_bytes_at_tick;

void metrics_init(void) {
    memset(&g_m, 0, sizeof(g_m));
    strncpy(g_m.formatName, "—", sizeof(g_m.formatName) - 1);
    g_m.bufferState = BUF_EMPTY;
}

AudioMetrics *metrics_get(void) {
    return &g_m;
}

void metrics_reset_session(void) {
    g_m.ramBufferSize = 0;
    g_m.ramBufferUsed = 0;
    g_m.bufferedSeconds = 0;
    g_m.bufferUsedPct = 0;
    g_m.networkBytesReceived = 0;
    g_m.networkRateMbps = 0;
    g_m.networkAvgMbps = 0;
    g_m.networkLatencyMs = 0;
    g_m.networkJitterMs = 0;
    g_m.flacDecodeRateMbps = 0;
    g_m.audioConsumptionMbps = 0;
    g_m.bufferUnderruns = 0;
    g_m.rebufferCount = 0;
    g_m.bufferState = BUF_EMPTY;
    g_net_tick = 0;
    g_net_bytes_at_tick = 0;
}

void metrics_set_buffer(unsigned size, unsigned used, float seconds) {
    g_m.ramBufferSize = size;
    g_m.ramBufferUsed = used;
    g_m.bufferedSeconds = seconds;
    g_m.bufferUsedPct = (size > 0) ? (100.0f * (float)used / (float)size) : 0.0f;
}

void metrics_set_format(const char *name, int lossless, int rate, int depth, int ch) {
    if (name) {
        strncpy(g_m.formatName, name, sizeof(g_m.formatName) - 1);
        g_m.formatName[sizeof(g_m.formatName) - 1] = '\0';
    }
    g_m.formatLossless = lossless ? 1 : 0;
    g_m.sampleRate = rate;
    g_m.bitDepth = depth;
    g_m.channels = ch;
}

void metrics_set_state(BufferState st) {
    g_m.bufferState = st;
}

void metrics_add_network_bytes(unsigned n, float instant_mbps) {
    unsigned now = sceKernelGetSystemTimeLow();
    g_m.networkBytesReceived += n;

    /* Auto-derive Mbps when caller passes 0 (stream path). */
    if (instant_mbps <= 0.0f && n > 0) {
        if (g_net_tick == 0) {
            g_net_tick = now;
            g_net_bytes_at_tick = g_m.networkBytesReceived;
        } else {
            unsigned dt = now - g_net_tick;
            if (dt >= 200000u) { /* ≥0.2s */
                unsigned long long db = g_m.networkBytesReceived - g_net_bytes_at_tick;
                instant_mbps = ((float)db * 8.0f) / ((float)dt);
                g_net_tick = now;
                g_net_bytes_at_tick = g_m.networkBytesReceived;
            }
        }
    }

    if (instant_mbps > 0.0f) {
        if (g_m.networkAvgMbps <= 0.0f) {
            g_m.networkAvgMbps = instant_mbps;
        } else {
            g_m.networkAvgMbps = g_m.networkAvgMbps * 0.85f + instant_mbps * 0.15f;
        }
        g_m.networkRateMbps = instant_mbps;
    }
}

void metrics_set_latency(float ms, float jitter_ms) {
    g_m.networkLatencyMs = ms;
    g_m.networkJitterMs = jitter_ms;
}

void metrics_set_decode_rate(float mbps) {
    g_m.flacDecodeRateMbps = mbps;
}

void metrics_set_consumption(float mbps) {
    g_m.audioConsumptionMbps = mbps;
}

void metrics_note_underrun(void) {
    g_m.bufferUnderruns++;
}

void metrics_note_rebuffer(void) {
    g_m.rebufferCount++;
}

int metrics_network_ok(float headroom) {
    if (headroom < 1.05f) {
        headroom = 1.25f;
    }
    if (g_m.audioConsumptionMbps <= 0.0f) {
        return 1;
    }
    return g_m.networkAvgMbps >= g_m.audioConsumptionMbps * headroom;
}

void metrics_format_diag(char *out, int out_sz) {
    if (!out || out_sz < 64) {
        return;
    }
    snprintf(
        out,
        out_sz,
        "AUDIO\n"
        "-----------------------\n"
        "FORMAT: %s\n"
        "LOSSLESS: %s\n"
        "SAMPLE RATE: %.1f kHz\n"
        "BIT DEPTH: %d bit\n"
        "BUFFER\n"
        "-----------------------\n"
        "SIZE: %.2f MB\n"
        "USED: %.0f%%\n"
        "BUFFERED: %.1f sec\n"
        "NETWORK\n"
        "-----------------------\n"
        "RATE: %.2f Mbps\n"
        "DECODER\n"
        "-----------------------\n"
        "RATE: %.2f Mbps\n"
        "STORAGE\n"
        "-----------------------\n"
        "REBUFFER\n"
        "-----------------------\n"
        "COUNT: %u\n",
        g_m.formatName,
        g_m.formatLossless ? "YES" : "NO",
        g_m.sampleRate / 1000.0f,
        g_m.bitDepth,
        g_m.ramBufferSize / (1024.0f * 1024.0f),
        g_m.bufferUsedPct,
        g_m.bufferedSeconds,
        g_m.networkRateMbps,
        g_m.flacDecodeRateMbps,
        g_m.rebufferCount
    );
}
