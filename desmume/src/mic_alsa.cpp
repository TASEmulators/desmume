#include <alsa/asoundlib.h>
#include "types.h"
#include "mic.h"

#define MIC_BUFSIZE 4096

BOOL Mic_Inited = FALSE;
u8 Mic_Buffer[2][MIC_BUFSIZE];
u16 Mic_BufPos;
u8 Mic_PlayBuf;
u8 Mic_WriteBuf;

int MicButtonPressed;

// Handle for the PCM device
static snd_pcm_t *pcm_handle;

BOOL Mic_Init()
{
    snd_pcm_hw_params_t *hwparams;

    if (Mic_Inited)
        return TRUE;

    // Open the default sound card in capture
    if (snd_pcm_open(&pcm_handle, "plughw:0,0", SND_PCM_STREAM_CAPTURE, 0) < 0)
        return FALSE;

    // Allocate the snd_pcm_hw_params_t structure and fill it.
    snd_pcm_hw_params_alloca(&hwparams);
    if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0)
        return FALSE;

    //Set the access
    if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
        return FALSE ;

    //dir 0 == exacte (Rate = 16K exacte)
    if (snd_pcm_hw_params_set_rate(pcm_handle, hwparams, 16000, 0) < 0)
        return FALSE;

    /* Set sample format */
    if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S8) < 0)
        return FALSE;

    // Set one channel (mono)
    if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 1) < 0)
        return FALSE;

    // Set 2 periods
    if (snd_pcm_hw_params_set_periods(pcm_handle, hwparams, 2, 0) < 0)
        return FALSE;

    // Set the buffer sise
     if (snd_pcm_hw_params_set_buffer_size(pcm_handle, hwparams, MIC_BUFSIZE) < 0)
        return FALSE;

    //Set the params
    if (snd_pcm_hw_params(pcm_handle, hwparams) < 0)
        return FALSE;

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

u8 Mic_ReadSample()
{
    int error;
    u8 tmp;
    u8 ret;

    if (!Mic_Inited)
        return 0;

    tmp = Mic_Buffer[Mic_PlayBuf][Mic_BufPos >> 1];

    if (Mic_BufPos & 0x1) {
        ret = ((tmp & 0x1) << 7);
    } else {
        ret = ((tmp & 0xFE) >> 1);
    }

    Mic_BufPos++;
    if (Mic_BufPos >= (MIC_BUFSIZE << 1)) {
        Mic_BufPos = 0;
        error = snd_pcm_readi(pcm_handle, Mic_Buffer[Mic_WriteBuf], MIC_BUFSIZE);
        if (error < 1) {
            printf("snd_pcm_readi FAIL!: %s\n", snd_strerror(error));
        }
        Mic_PlayBuf ^= 1;
        Mic_WriteBuf ^= 1;
    }

    return ret;
}
