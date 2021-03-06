/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v27ter_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: v27ter_tests.c,v 1.70 2006/11/19 14:07:27 steveu Exp $
 */

/*! \page v27ter_tests_page V.27ter modem tests
\section v27ter_tests_page_sec_1 What does it do?
These tests test one way paths, as V.27ter is a half-duplex modem. They allow either:

 - A V.27ter transmit modem to feed a V.27ter receive modem through a telephone line
   model. BER testing is then used to evaluate performance under various line
   conditions. This is effective for testing the basic performance of the
   receive modem. It is also the only test mode provided for evaluating the
   transmit modem.

 - A V.27ter receive modem is used to decode V.27ter audio, stored in a wave file.
   This is good way to evaluate performance with audio recorded from other
   models of modem, and with real world problematic telephone lines.

If the appropriate GUI environment exists, the tests are built such that a visual
display of modem status is maintained.

\section v27ter_tests_page_sec_2 How is it used?
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)  &&  defined(HAVE_FL_FL_AUDIO_METER_H)
#define ENABLE_GUI
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <assert.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"
#include "test_utils.h"
#include "line_model.h"
#if defined(ENABLE_GUI)
#include "modem_monitor.h"
#include "line_model_monitor.h"
#endif

#define BLOCK_LEN       160

#define OUT_FILE_NAME   "v27ter.wav"

char *decode_test_file = NULL;
int use_gui = FALSE;

int symbol_no = 0;

int rx_bits = 0;

bert_state_t bert;
one_way_line_model_state_t *line_model;

#if defined(ENABLE_GUI)
qam_monitor_t *qam_monitor;
#endif

bert_results_t latest_results;

static void reporter(void *user_data, int reason, bert_results_t *results)
{
    switch (reason)
    {
    case BERT_REPORT_SYNCED:
        printf("BERT report synced\n");
        break;
    case BERT_REPORT_UNSYNCED:
        printf("BERT report unsync'ed\n");
        break;
    case BERT_REPORT_REGULAR:
        printf("BERT report regular - %d bits, %d bad bits, %d resyncs\n", results->total_bits, results->bad_bits, results->resyncs);
        memcpy(&latest_results, results, sizeof(latest_results));
        break;
    case BERT_REPORT_GT_10_2:
        printf("BERT report > 1 in 10^2\n");
        break;
    case BERT_REPORT_LT_10_2:
        printf("BERT report < 1 in 10^2\n");
        break;
    case BERT_REPORT_LT_10_3:
        printf("BERT report < 1 in 10^3\n");
        break;
    case BERT_REPORT_LT_10_4:
        printf("BERT report < 1 in 10^4\n");
        break;
    case BERT_REPORT_LT_10_5:
        printf("BERT report < 1 in 10^5\n");
        break;
    case BERT_REPORT_LT_10_6:
        printf("BERT report < 1 in 10^6\n");
        break;
    case BERT_REPORT_LT_10_7:
        printf("BERT report < 1 in 10^7\n");
        break;
    default:
        printf("BERT report reason %d\n", reason);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void v27terputbit(void *user_data, int bit)
{
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            printf("Training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            printf("Training succeeded\n");
            break;
        case PUTBIT_CARRIER_UP:
            printf("Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            printf("Carrier down\n");
            break;
        default:
            printf("Eh!\n");
            break;
        }
        return;
    }
    if (decode_test_file)
        printf("Rx bit %d - %d\n", rx_bits++, bit);
    else
        bert_put_bit(&bert, bit);
}
/*- End of function --------------------------------------------------------*/

static int v27tergetbit(void *user_data)
{
    return bert_get_bit(&bert);
}
/*- End of function --------------------------------------------------------*/

static void qam_report(void *user_data, const complexf_t *constel, const complexf_t *target, int symbol)
{
    int i;
    int len;
    complexf_t *coeffs;
    float fpower;
    float error;
    v27ter_rx_state_t *rx;
    static float smooth_power = 0.0;
#if defined(ENABLE_GUI)
    static int reports = 0;
#endif

    rx = (v27ter_rx_state_t *) user_data;
    if (constel)
    {
        fpower = (constel->re - target->re)*(constel->re - target->re)
               + (constel->im - target->im)*(constel->im - target->im);
        smooth_power = 0.95*smooth_power + 0.05*fpower;
#if defined(ENABLE_GUI)
        if (use_gui)
        {
            qam_monitor_update_constel(qam_monitor, constel);
            qam_monitor_update_carrier_tracking(qam_monitor, v27ter_rx_carrier_frequency(rx));
            qam_monitor_update_symbol_tracking(qam_monitor, v27ter_rx_symbol_timing_correction(rx));
        }
#endif
        error = constel->im*target->re - constel->re*target->im;
        printf("Tracking error %f %f %f %f %f %f\n", error, v27ter_rx_carrier_frequency(rx), constel->re, constel->im, target->re, target->im);
        printf("%8d [%8.4f, %8.4f] [%8.4f, %8.4f] %8.4f %8.4f %9.4f %7.3f\n",
               symbol_no,
               constel->re,
               constel->im,
               target->re,
               target->im,
               fpower,
               smooth_power,
               v27ter_rx_carrier_frequency(rx),
               v27ter_rx_signal_power(rx));
        len = v27ter_rx_equalizer_state(rx, &coeffs);
        printf("Equalizer B:\n");
        for (i = 0;  i < len;  i++)
            printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, powerf(&coeffs[i]));
        printf("Gardtest %d %f %d\n", symbol_no, v27ter_rx_symbol_timing_correction(rx), rx->gardner_integrate);
        printf("Carcar %d %f\n", symbol_no, v27ter_rx_carrier_frequency(rx));
#if defined(ENABLE_GUI)
        if (use_gui)
        {
            if (++reports >= 1000)
            {
                qam_monitor_update_equalizer(qam_monitor, coeffs, len);
                reports = 0;
            }
        }
#endif
        symbol_no++;
    }
    else
    {
        printf("Gardner step %d\n", symbol);
        len = v27ter_rx_equalizer_state(rx, &coeffs);
        printf("Equalizer A:\n");
        for (i = 0;  i < len;  i++)
            printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, powerf(&coeffs[i]));
#if defined(ENABLE_GUI)
        if (use_gui)
            qam_monitor_update_equalizer(qam_monitor, coeffs, len);
#endif
    }
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    v27ter_rx_state_t rx;
    v27ter_tx_state_t tx;
    bert_results_t bert_results;
    int16_t gen_amp[BLOCK_LEN];
    int16_t amp[BLOCK_LEN];
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int outframes;
    int samples;
    int i;
    int tep;
    int test_bps;
    int noise_level;
    int signal_level;
    int bits_per_test;
    int line_model_no;
    int block;
    int log_audio;
    int channel_codec;

    channel_codec = MUNGE_CODEC_NONE;
    test_bps = 4800;
    tep = FALSE;
    line_model_no = 0;
    decode_test_file = NULL;
    use_gui = FALSE;
    noise_level = -70;
    signal_level = -13;
    bits_per_test = 50000;
    log_audio = FALSE;
    for (i = 1;  i < argc;  i++)
    {
        if (strcmp(argv[i], "-b") == 0)
        {
            bits_per_test = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "-c") == 0)
        {
            channel_codec = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "-d") == 0)
        {
            decode_test_file = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-t") == 0)
        {
            tep = TRUE;
            continue;
        }
        if (strcmp(argv[i], "-g") == 0)
        {
            use_gui = TRUE;
            continue;
        }
        if (strcmp(argv[i], "-l") == 0)
        {
            log_audio = TRUE;
            continue;
        }
        if (strcmp(argv[i], "-m") == 0)
        {
            line_model_no = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "-n") == 0)
        {
            noise_level = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "-s") == 0)
        {
            signal_level = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "4800") == 0)
            test_bps = 4800;
        else if (strcmp(argv[i], "2400") == 0)
            test_bps = 2400;
        else
        {
            fprintf(stderr, "Invalid bit rate\n");
            exit(2);
        }
    }
    inhandle = NULL;
    outhandle = NULL;

    filesetup = AF_NULL_FILESETUP;
    if (log_audio)
    {
        if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
        {
            fprintf(stderr, "    Failed to create file setup\n");
            exit(2);
        }
        afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
        afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
        afInitFileFormat(filesetup, AF_FILE_WAVE);
        afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);
        if ((outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
    }

    if (decode_test_file)
    {
        /* We will decode the audio from a wave file. */
        if ((inhandle = afOpenFile(decode_test_file, "r", NULL)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot open wave file '%s'\n", decode_test_file);
            exit(2);
        }
    }
    else
    {
        /* We will generate V.27ter audio, and add some noise to it. */
        v27ter_tx_init(&tx, test_bps, tep, v27tergetbit, NULL);
        v27ter_tx_power(&tx, signal_level);
        /* Move the carrier off a bit */
        tx.carrier_phase_rate = dds_phase_ratef(1810.0f);

        bert_init(&bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
        bert_set_report(&bert, 10000, reporter, NULL);

        if ((line_model = one_way_line_model_init(line_model_no, (float) noise_level, channel_codec)) == NULL)
        {
            fprintf(stderr, "    Failed to create line model\n");
            exit(2);
        }
    }

    v27ter_rx_init(&rx, test_bps, v27terputbit, NULL);
    v27ter_rx_set_qam_report_handler(&rx, qam_report, (void *) &rx);
    span_log_set_level(&rx.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(&rx.logging, "V.27ter-rx");

#if defined(ENABLE_GUI)
    if (use_gui)
    {
        qam_monitor = qam_monitor_init(2.0, NULL);
        if (!decode_test_file)
        {
            start_line_model_monitor(129);
            line_model_monitor_line_model_update(line_model->near_filter, line_model->near_filter_len);
        }
    }
#endif

    memset(&latest_results, 0, sizeof(latest_results));
    for (block = 0;  ;  block++)
    {
        if (decode_test_file)
        {
            samples = afReadFrames(inhandle,
                                   AF_DEFAULT_TRACK,
                                   amp,
                                   BLOCK_LEN);
#if defined(ENABLE_GUI)
            if (use_gui)
                qam_monitor_update_audio_level(qam_monitor, amp, samples);
#endif
            if (samples == 0)
                break;
        }
        else
        {
            samples = v27ter_tx(&tx, gen_amp, BLOCK_LEN);
#if defined(ENABLE_GUI)
            if (use_gui)
                qam_monitor_update_audio_level(qam_monitor, gen_amp, samples);
#endif
            if (samples == 0)
            {
                printf("Restarting on zero output\n");

                /* Push a little silence through, to ensure all the data bits get out of the buffers */
                memset(amp, 0, BLOCK_LEN*sizeof(int16_t));
                v27ter_rx(&rx, amp, BLOCK_LEN);
                v27ter_rx(&rx, amp, BLOCK_LEN);
                v27ter_rx(&rx, amp, BLOCK_LEN);

                /* Note that we might get a few bad bits as the carrier shuts down. */
                bert_result(&bert, &bert_results);
                fprintf(stderr, "Final result %ddBm0, %d bits, %d bad bits, %d resyncs\n", signal_level, bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
                fprintf(stderr, "Last report  %ddBm0, %d bits, %d bad bits, %d resyncs\n", signal_level, latest_results.total_bits, latest_results.bad_bits, latest_results.resyncs);
                /* See if bit errors are appearing yet. Also check we are getting enough bits out of the receiver. The last regular report
                   should be error free, though the final report will generally contain bits errors as the carrier was dying. The total
                   number of bits out of the receiver should be at least the number we sent. Also, since BERT sync should have occurred
                   rapidly at the start of transmission, the last report should have occurred at not much less than the total number of
                   bits we sent. */
                if (bert_results.total_bits < bits_per_test
                    ||
                    latest_results.total_bits < bits_per_test - 100
                    ||
                    latest_results.bad_bits != 0)
                {
                    break;
                }
                memset(&latest_results, 0, sizeof(latest_results));
                signal_level--;
                v27ter_tx_restart(&tx, test_bps, tep);
                v27ter_tx_power(&tx, signal_level);
                v27ter_rx_restart(&rx, test_bps, FALSE);
                bert_init(&bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
                bert_set_report(&bert, 10000, reporter, NULL);
                one_way_line_model_release(line_model);
                if ((line_model = one_way_line_model_init(line_model_no, (float) noise_level, channel_codec)) == NULL)
                {
                    fprintf(stderr, "    Failed to create line model\n");
                    exit(2);
                }
            }

            if (log_audio)
            {
                outframes = afWriteFrames(outhandle,
                                          AF_DEFAULT_TRACK,
                                          gen_amp,
                                          samples);
                if (outframes != samples)
                {
                    fprintf(stderr, "    Error writing wave file\n");
                    exit(2);
                }
            }
            one_way_line_model(line_model, amp, gen_amp, samples);
        }
#if defined(ENABLE_GUI)
        if (use_gui  &&  !decode_test_file)
            line_model_monitor_line_spectrum_update(amp, samples);
#endif
        v27ter_rx(&rx, amp, samples);
        if (decode_test_file == NULL  &&  block%500 == 0)
            printf("Noise level is %d\n", noise_level);
    }
    if (decode_test_file)
    {
#if defined(ENABLE_GUI)
        if (use_gui)
            qam_wait_to_end(qam_monitor);
#endif
    }
    else
    {
        bert_result(&bert, &bert_results);
        fprintf(stderr, "At completion:\n");
        fprintf(stderr, "Final result %ddBm0, %d bits, %d bad bits, %d resyncs\n", signal_level, bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
        fprintf(stderr, "Last report  %ddBm0, %d bits, %d bad bits, %d resyncs\n", signal_level, latest_results.total_bits, latest_results.bad_bits, latest_results.resyncs);
        one_way_line_model_release(line_model);
        if (signal_level > -43)
        {
            printf("Tests failed.\n");
            exit(2);
        }

        printf("Tests passed.\n");
    }
    if (log_audio)
    {
        if (afCloseFile(outhandle))
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
        afFreeFileSetup(filesetup);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
