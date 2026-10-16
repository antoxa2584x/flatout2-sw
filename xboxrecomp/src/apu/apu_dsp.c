/*
 * MCPX APU DSP (GP/EP) - Stub implementation
 *
 * The DSP Global Processor (GP) and Encode Processor (EP) handle effects
 * processing (reverb, chorus, etc.) and final output encoding. The full
 * DSP is ~3000 lines of DSP56300 emulation code.
 *
 * For initial audio, we bypass the DSP entirely:
 * - VP mixbins are passed directly to the EP output
 * - GP effects processing is skipped
 * - The EP just copies mixbin 0/1 (front L/R) to the monitor buffer
 *
 * This gives us basic voice playback without effects. The DSP can be
 * connected later for reverb, EQ, and other processing.
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2019-2025 Matt Borgerson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "apu_state.h"
#include "fpconv.h"
#include "apu_xaudio2.h"

#include <stdlib.h>
#include <string.h>

/* ── DSP command doorbell acknowledgement ────────────────────────────────
 *
 * DirectSound does not stop at creating the device. It hands the audio DSP a
 * command block in guest RAM, writes a command word, and spins until the DSP
 * writes zero back. On real hardware the GP runs a DSP56300 program that does
 * that. Here the DSP is a passthrough stub, so the word never changes and the
 * title hangs inside DirectSound initialisation -- which on Wreckless gates the
 * entire engine, not just audio.
 *
 * RECOMP_APU_DSP_ACK=<addr>[,<addr>...] clears those guest dwords once per APU
 * frame, which is what "the command completed" looks like to the title.
 *
 * ponytail: this is a handshake acknowledgement, not a DSP. It says every
 * command succeeded instantly and computes nothing, so anything whose *result*
 * the title reads back will still be wrong. The real fix is DSP56300 emulation
 * in the GP/EP; this exists so audio init stops blocking everything behind it.
 *
 * The address is not derivable from the APU registers: GPSADDR/GPFADDR/
 * EPSADDR/EPFADDR point at the DSP's own scratch and frame memory, while the
 * command block is a DirectSound heap allocation. On Wreckless the registers
 * read 0x01504000 / 0x014EC000 / 0x0151C000 / 0x014F0000 and the doorbell is at
 * 0x014F8810 -- inside none of them. So it has to be observed: run with
 * RECOMP_WATCHDOG_SECS and the spin shows up as ebx plus the poll offset.
 */
#define APU_DSP_ACK_MAX 8
static uint32_t s_dsp_ack[APU_DSP_ACK_MAX];
static int s_dsp_ack_count = -1;

static void dsp_ack_init(void)
{
    const char *spec = getenv("RECOMP_APU_DSP_ACK");
    char buf[256], *p, *end;

    s_dsp_ack_count = 0;
    if (!spec || !*spec)
        return;
    strncpy(buf, spec, sizeof buf - 1);
    buf[sizeof buf - 1] = 0;
    for (p = buf; *p && s_dsp_ack_count < APU_DSP_ACK_MAX; ) {
        unsigned long v = strtoul(p, &end, 0);
        if (end == p)
            break;
        if (v)
            s_dsp_ack[s_dsp_ack_count++] = (uint32_t)v;
        p = (*end == ',') ? end + 1 : end;
    }
    if (s_dsp_ack_count)
        fprintf(stderr, "[APU] DSP doorbell ack: %d address(es), first 0x%08X\n",
                s_dsp_ack_count, s_dsp_ack[0]);
}

/* SUM EVERY MIXBIN THE GUEST ROUTED TO, NOT JUST THE FIRST TWO.
 *
 * Default ON. RECOMP_APU_MIXDOWN_ALL=0 restores the previous two-bin read,
 * because this changes audible output for every title and an escape hatch
 * costs one branch. */
static int mcpx_apu_mixdown_all(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RECOMP_APU_MIXDOWN_ALL");
        on = (e && *e) ? (atoi(e) != 0) : 1;
    }
    return on;
}

/* Stereo downmix gains per mixbin (DSMIXBIN layout). Even/odd alone put
 * the centre channel on the left and the LFE on the right: NFSU2's EA mixer
 * writes engine and speech to its C/LFE voice (v0F5, bins 2/3), which was
 * then heard only on the left. Centre, LFE and the mono I3DL2 send go to
 * both sides at -3 dB; the other bins keep their even-left/odd-right pairing
 * (FX sends 11..31 have no fixed position). */
#define DM_M3DB 0.70710678f
static float s_downmix[NUM_MIXBINS][2];

static void downmix_init(void)
{
    for (int b = 0; b < NUM_MIXBINS; ++b) {
        s_downmix[b][0] = (b & 1) ? 0.0f : 1.0f;
        s_downmix[b][1] = (b & 1) ? 1.0f : 0.0f;
    }
    s_downmix[2][0] = s_downmix[2][1] = DM_M3DB;    /* front centre */
    s_downmix[3][0] = s_downmix[3][1] = DM_M3DB;    /* LFE */
    s_downmix[10][0] = s_downmix[10][1] = DM_M3DB;  /* I3DL2 reverb send */
    /* RECOMP_APU_DOWNMIX="bin:left:right,...": a title's own gains. With
     * the GP/EP stubbed nothing applies what the title's DSP image would to
     * its bins. FlatOut 2 (FMOD, fmodfx.bin) sends every effect to 6/7 and
     * the same signal at half level to the reverb send 10; mixed dry, the
     * race sum went past full scale and the tanh below crushed it. */
    {
        const char *e = getenv("RECOMP_APU_DOWNMIX");
        while (e && *e) {
            char *end;
            int b = (int)strtol(e, &end, 10);
            float l, r;
            if (end == e || *end != ':') break;
            l = strtof(end + 1, &end);
            if (*end != ':') break;
            r = strtof(end + 1, &end);
            if (b >= 0 && b < NUM_MIXBINS) {
                s_downmix[b][0] = l;
                s_downmix[b][1] = r;
            }
            e = *end == ',' ? end + 1 : NULL;
        }
        if (getenv("RECOMP_APU_DOWNMIX"))
            fprintf(stderr, "[APU] downmix: %s\n", getenv("RECOMP_APU_DOWNMIX"));
    }
}

static void dsp_ack_frame(MCPXAPUState *d)
{
    int i;

    if (s_dsp_ack_count < 0)
        dsp_ack_init();
    if (!d->ram_ptr)
        return;
    for (i = 0; i < s_dsp_ack_count; i++) {
        uint32_t *slot = (uint32_t *)(d->ram_ptr + s_dsp_ack[i]);
        if (*slot) {
            static int shown[APU_DSP_ACK_MAX];
            if (shown[i]++ < 3)
                fprintf(stderr, "[APU] DSP doorbell 0x%08X: command 0x%08X"
                                " acknowledged\n", s_dsp_ack[i], *slot);
            *slot = 0;
        }
    }
}

void mcpx_apu_dsp_init(MCPXAPUState *d)
{
    /* Allocate minimal DSP state for GP and EP.
     * We need these to exist so reset doesn't crash,
     * but they won't actually run DSP programs. */
    d->gp.dsp = (DSPState *)calloc(1, sizeof(DSPState));
    d->ep.dsp = (DSPState *)calloc(1, sizeof(DSPState));

    if (d->gp.dsp) d->gp.dsp->is_gp = true;
    if (d->ep.dsp) d->ep.dsp->is_gp = false;

    d->gp.realtime = false;
    d->ep.realtime = false;
    downmix_init();

    fprintf(stderr, "[APU] DSP GP/EP initialized (STUBBED - passthrough mode)\n");
}

void mcpx_apu_update_dsp_preference(MCPXAPUState *d)
{
    /* In the real xemu, this reads settings to decide whether
     * GP/EP should run in realtime or cached mode. We ignore it. */
    (void)d;
}

/* RECOMP_APU_BIN_STATS=1: RMS and peak of each mixbin every 5 s of audio
 * (which bins a title's voices land in, and how loud each is). */
static void bin_stats(float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME], int i)
{
    static int on = -1;
    static double sq[NUM_MIXBINS];
    static float pk[NUM_MIXBINS];
    static unsigned n;
    int b;
    if (on < 0) {
        const char *e = getenv("RECOMP_APU_BIN_STATS");
        on = e && *e == '1';
    }
    if (!on)
        return;
    for (b = 0; b < NUM_MIXBINS; b++) {
        float v = mixbins[b][i];
        sq[b] += (double)v * v;
        if (fabsf(v) > pk[b]) pk[b] = fabsf(v);
    }
    if (++n >= 48000u * 5) {
        fprintf(stderr, "[APU-BINS]");
        for (b = 0; b < NUM_MIXBINS; b++)
            if (pk[b] > 0.0f)
                fprintf(stderr, " %d:%.3f/%.2f", b, sqrt(sq[b] / n), pk[b]);
        fprintf(stderr, "\n");
        memset(sq, 0, sizeof sq);
        memset(pk, 0, sizeof pk);
        n = 0;
    }
}

/* Frame-boundary jumps per mix bin (RECOMP_APU_RING_STATS), read and reset
 * by apu_vp.c's 10 s report. */
uint32_t g_bin_jumps[NUM_MIXBINS];

/* Non-finite mixdown samples silenced since the last call (see below). */
static volatile uint32_t s_nonfinite;

uint32_t mcpx_apu_nonfinite(void)
{
    uint32_t n = s_nonfinite;
    s_nonfinite = 0;            /* ponytail: racy, a counter only */
    return n;
}

void mcpx_apu_dsp_frame(MCPXAPUState *d,
                         float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME])
{
    /* Bypass DSP: take mixbin 0 (front-left) and mixbin 1 (front-right)
     * and write them directly to the monitor frame buffer as the final
     * EP output.
     *
     * The Xbox DirectSound typically routes:
     *   Mixbin 0 = Front Left
     *   Mixbin 1 = Front Right
     *   Mixbin 2 = Center (often unused in stereo)
     *   Mixbin 3 = LFE
     *   Mixbin 4-5 = Rear L/R
     *
     * For stereo output, bins 0 and 1 are what we want.
     */

    int off = (d->ep_frame_div % 8) * NUM_SAMPLES_PER_FRAME;

    dsp_ack_frame(d);

    {   /* RECOMP_APU_RING_STATS: which mix bins jump at a frame boundary */
        static int on = -1;
        static float last[NUM_MIXBINS], stepavg[NUM_MIXBINS];
        if (on < 0) on = getenv("RECOMP_APU_RING_STATS") != NULL;
        if (on) {
            for (int b = 0; b < NUM_MIXBINS; b++) {
                float st = 0.0f, j = fabsf(mixbins[b][0] - last[b]);
                for (int i = 1; i < NUM_SAMPLES_PER_FRAME; i++)
                    st += fabsf(mixbins[b][i] - mixbins[b][i - 1]);
                st /= (float)(NUM_SAMPLES_PER_FRAME - 1);
                stepavg[b] = stepavg[b] * 0.98f + st * 0.02f;
                if (j > 0.05f && j > 12.0f * stepavg[b] + 0.01f)
                    g_bin_jumps[b]++;
                last[b] = mixbins[b][NUM_SAMPLES_PER_FRAME - 1];
            }
        }
    }

    if (d->monitor.point != MCPX_APU_DEBUG_MON_VP) {
        static ApuLimiter lim;
        float lr[NUM_SAMPLES_PER_FRAME][2];
        for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
            /* Bins 2..31 used to be computed and then dropped on the floor.
             * On hardware the GP and EP mix the submixes down; here they are
             * stubs, so thirty of thirty-two bins were discarded every frame
             * with no counter anywhere to say so.
             *
             * Measured on Jet Set Radio Future, one 200 s gameplay run, with
             * a positive control moving beside it:
             *
             *     [APU-BIN] 2D heard=557466 lost=0
             *               3D heard=0      lost=377768
             *               lost by bin: 6,7,8,9,10
             *
             * 557,466 music voice-frames heard and none lost; 377,768 effect
             * voice-frames produced correctly and thrown away. The title's 3D
             * positional voices -- its sound effects -- are routed to bins 6
             * to 10 by the guest's own V0BIN..V3BIN, and music on 2D voices
             * lands in bins 0 and 1, which is why the music was always
             * audible and no effect ever was. Every instrument upstream of
             * this line read healthy.
             *
             * Gating the HRTF submix override was tried first and did not fix
             * it, so the defect is the width of this mixdown and nothing else.
             *
             * Even bins left, odd bins right, which preserves the stereo
             * pairing the guest set up -- bins 6/7 and 8/9 arrive with matched
             * counts -- except centre, LFE and I3DL2, which go to both sides
             * (s_downmix). This is not what a real EP does; it is the cheapest
             * mixdown that stops discarding audio. */
            float left, right;
            bin_stats(mixbins, i);
            if (mcpx_apu_mixdown_all()) {
                left = 0.0f;
                right = 0.0f;
                for (int b = 0; b < NUM_MIXBINS; ++b) {
                    left  += s_downmix[b][0] * mixbins[b][i];
                    right += s_downmix[b][1] * mixbins[b][i];
                }
            } else {
                left = mixbins[0][i];
                right = mixbins[1][i];
            }
            /* A NaN or infinity from any voice (a NaN position or gain
             * upstream) came out as a sample of 0 -- a pop -- and sat in
             * the limiter's delay line meanwhile. Silence it, and count it
             * (mcpx_apu_nonfinite) so a log says where pops come from. */
            if (!isfinite(left))  { left = 0.0f;  s_nonfinite++; }
            if (!isfinite(right)) { right = 0.0f; s_nonfinite++; }
            lr[i][0] = left;
            lr[i][1] = right;
        }
        /* Summing every bin goes past full scale (1.33 in the intro movies,
         * 1.27 in a FlatOut 2 crash), and a hard clamp there is audible
         * distortion. The limiter turns the gain down instead (tanh here
         * used to bend the waveform itself: overdrive on loud effects). */
        apu_limiter_run(&lim, &lr[0][0], NUM_SAMPLES_PER_FRAME, 1.0f);

        /* Convert to 16-bit and write (not accumulate) into frame buffer.
         * Each of the 8 sub-frames writes its own 32-sample slice. */
        for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
            d->monitor.frame_buf[off + i][0] = (int16_t)lrintf(lr[i][0] * 32767.0f);
            d->monitor.frame_buf[off + i][1] = (int16_t)lrintf(lr[i][1] * 32767.0f);
        }
    }

    g_dbg.gp.cycles = 0;
    g_dbg.ep.cycles = 0;
}
