/* mic_alsa.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2009 DeSmuME Team
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <alsa/asoundlib.h>
#include <glib.h>
#include "types.h"
#include "mic.h"
#include "debug.h"
#include "readwrite.h"
#include "emufile.h"

#define MIC_BUFSIZE 4096

BOOL Mic_Inited = FALSE;
u8 Mic_Buffer[2][MIC_BUFSIZE];
u16 Mic_BufPos;
u8 Mic_PlayBuf;
u8 Mic_WriteBuf;

int MicButtonPressed;

/* Alsa stuff */
static snd_pcm_t *pcm_handle;

BOOL Mic_Init()
{
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_uframes_t mic_bufsize, periods;
    int err;

    if (Mic_Inited)
        return TRUE;

    if ((err = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK)) < 0) {
        g_printerr("Failed to open device: %s\n", snd_strerror(err));
        return FALSE;
    }

    /* Hardware params */
    snd_pcm_hw_params_alloca(&hwparams);
    if ((err = snd_pcm_hw_params_any(pcm_handle, hwparams)) < 0) {
        g_printerr("Failed to setup hw parameters: %s\n", snd_strerror(err));
        return FALSE;
    }

    if ((err = snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        g_printerr("Failed to set access: %s\n", snd_strerror(err));
        return FALSE;
    }

    /* 8bit signed, mono, 16000hz */
    if ((err = snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S8)) < 0) {
        g_printerr("Failed to set format: %s\n", snd_strerror(err));
        return FALSE;
    }

    if ((err = snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 1)) < 0) {
        g_printerr("Failed to set channels: %s\n", snd_strerror(err));
        return FALSE;
    }

    if ((err = snd_pcm_hw_params_set_rate(pcm_handle, hwparams, 16000, 0)) < 0) {
        g_printerr("Failed to set rate: %s\n", snd_strerror(err));
        return FALSE;
    }

    if ((err = snd_pcm_hw_params(pcm_handle, hwparams)) < 0) {
        g_printerr("Failed to set hw parameters: %s\n", snd_strerror(err));
        return FALSE;
    }

    /* Query the driver for buffer and period sizes */
    if ((err = snd_pcm_hw_params_get_buffer_size(hwparams, &mic_bufsize)) < 0) {
        g_printerr("Failed to get buffer size: %s\n", snd_strerror(err));
        return FALSE;
    }

    if ((err = snd_pcm_hw_params_get_period_size(hwparams, &periods, 0)) < 0) {
        g_printerr("Failed to get period size: %s\n", snd_strerror(err));
        return FALSE;
    }

    Mic_Inited = TRUE;
    Mic_Reset();

    return TRUE;
}

void Mic_Reset()
{
    if (!Mic_Inited)
        return;

    memset(Mic_Buffer, 0, MIC_BUFSIZE * 2);
    Mic_BufPos = 0;
    Mic_PlayBuf = 1;
    Mic_WriteBuf = 0;
}

void Mic_DeInit()
{
    if (!Mic_Inited)
        return;

    Mic_Inited = FALSE;

    snd_pcm_drop(pcm_handle);
    snd_pcm_close(pcm_handle);
}

static void snd_pcm_read()
{
    int error;

    error = snd_pcm_readi(pcm_handle, Mic_Buffer[Mic_WriteBuf], MIC_BUFSIZE);
    if (error < 0)
        error = snd_pcm_recover(pcm_handle, error, 0);
    if (error < 0)
       LOG("snd_pcm_readi FAIL!: %s\n", snd_strerror(error));
}

u8 Mic_ReadSample()
{
    u8 tmp;
    u8 ret;

    if (!Mic_Inited)
        return 0;

    tmp = Mic_Buffer[Mic_PlayBuf][Mic_BufPos >> 1];

    if (Mic_BufPos & 0x1) {
        ret = ((tmp & 0x1) << 7);
    } else {
        ret = ((tmp & 0xFE) >> 1);
        snd_pcm_read();
    }

    Mic_BufPos++;
    if (Mic_BufPos >= (MIC_BUFSIZE << 1)) {
        snd_pcm_read();
        Mic_BufPos = 0;
        Mic_PlayBuf ^= 1;
        Mic_WriteBuf ^= 1;
    }

    return ret;
}

/* FIXME: stub! */
void mic_savestate(EMUFILE* os)
{
    write32le((u32)(-1),os);
}

bool mic_loadstate(EMUFILE* is, int size) 
{
    is->fseek(size, SEEK_CUR);
    return TRUE;
}
