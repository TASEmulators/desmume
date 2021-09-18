
#include "NDSSystem.h"
#include "MMU.h"
#include "rasterize.h"
#include "SPU.h"
#include "../src/utils/colorspacehandler/colorspacehandler.h"
#include "mc.h"
#include <emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

EMUFILE_MEMORY *savFile = new EMUFILE_MEMORY();
u8 *romBuffer;
int romBufferCap = 0;
int romLen;
volatile bool execute = true;
volatile bool paused = false;
// static s16 samplesBuffer[16384 * 2];
static s16 audioBuffer[16384 * 2];
int samplesRead = 0;
int samplesDesired = 0;

void SNDWasmUpdateAudio(s16 *buffer, u32 num_samples) {
  // printf("%d\n", num_samples);
  samplesRead = num_samples;
  memcpy(audioBuffer, buffer, sizeof(s16) * num_samples * 2);
}
u32 SNDWasmGetAudioSpace() { return samplesDesired; }
int SNDWasmInit(int buffersize) { return 0; }
void SNDWasmDeInit() {}
void SNDWasmMuteAudio() {}
void SNDWasmUnMuteAudio() {}
void SNDWasmSetVolume(int volume) {}
void SNDWasmClearBuffer() {}
void SNDWasmFetchSamples(s16 *sampleBuffer, size_t sampleCount,
                         ESynchMode synchMode,
                         ISynchronizingAudioBuffer *theSynchronizer) {
  if (synchMode == ESynchMode_Synchronous) {
    theSynchronizer->enqueue_samples(sampleBuffer, sampleCount);
  }
}

SoundInterface_struct SndWasm = {1,
                                 "Wasm Sound Interface",
                                 SNDWasmInit,
                                 SNDWasmDeInit,
                                 SNDWasmUpdateAudio,
                                 SNDWasmGetAudioSpace,
                                 SNDWasmMuteAudio,
                                 SNDWasmUnMuteAudio,
                                 SNDWasmSetVolume,
                                 SNDWasmClearBuffer,
                                 NULL,
                                 NULL};

SoundInterface_struct *SNDCoreList[] = {&SNDDummy, &SndWasm, NULL};
GPU3DInterface *core3DList[] = {&gpu3DNull, &gpu3DRasterize, NULL};

extern "C" {

u32 dstFrameBuffer[2][256 * 192];

static void gpu_screen_to_rgb(u32 *dst) {
  ColorspaceConvertBuffer555To8888Opaque<false, false>(
      (const uint16_t *)GPU->GetDisplayInfo().masterNativeBuffer, dst,
      GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
}

unsigned cpu_features_get_core_amount(void) { return 1; }

int main() {
  NDS_Init();
  SPU_ChangeSoundCore(1, 16384);

  GPU->Change3DRendererByID(RENDERID_SOFTRASTERIZER);
  printf("wasm ready.\n");
  EM_ASM(wasmReady(););
}

void *prepareRomBuffer(int rl) {
  romLen = rl;
  if (romLen > romBufferCap) {
    romBuffer = (u8 *)realloc((void *)romBuffer, romLen);
    romBufferCap = romLen;
  }
  return romBuffer;
}

void *getSymbol(int id) {
  if (id == 0) {
    // return NDS::ARM7BIOS;
  }
  if (id == 1) {
    // return NDS::ARM9BIOS;
  }
  if (id == 2) {
    // return SPI_Firmware::Firmware;
  }
  if (id == 3) {
    // return NDSCart::CartROM;
  }
  if (id == 4) {
    return dstFrameBuffer;
  }

  if (id == 6) {
    return audioBuffer;
  }
  if (id == 7) {
    return MMU.MAIN_MEM;
  }
  return 0;
}

void reset() { NDS_Reset(); }

int loadROM(int romLen) {
  int ret = NDS_LoadROM("rom.nds");
  SPU_SetSynchMode(ESynchMode_Synchronous, ESynchMethod_N);
  SPU_SetVolume(100);
  return ret;
}

static inline void convertFB(u8 *dst, u8 *src) {
  for (int i = 0; i < 256 * 192; i++) {
    uint8_t r = src[2];
    uint8_t g = src[1];
    uint8_t b = src[0];
    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = 0xFF;
    dst += 4;
    src += 4;
  }
}

int savGetSize() { return savFile->size(); }

void *savGetPointer(int desiredSize) {
  if (desiredSize > 0) {
    savFile->truncate(desiredSize);
    savFile->fseek(0, SEEK_SET);
  }
  return savFile->buf();
}

int savUpdateChangeFlag() {

  int ret = (savFile->changed) ? 1 : 0;
  savFile->changed = 0;
  return ret;
}

int runFrame(int shouldDraw, u32 keys, int touched, u32 touchX, u32 touchY) {
  if (!shouldDraw) {
    NDS_SkipNextFrame();
  }
  if (touched) {
    NDS_setTouchPos(touchX, touchY);
  } else {
    NDS_releaseTouch();
  }
  NDS_setPad(keys & (1 << 0), keys & (1 << 1), keys & (1 << 2), keys & (1 << 3),
             keys & (1 << 4), keys & (1 << 5), keys & (1 << 6), keys & (1 << 7),
             keys & (1 << 8), keys & (1 << 9), keys & (1 << 10),
             keys & (1 << 11), keys & (1 << 12), keys & (1 << 13));
  int ret = 0;
  NDS_beginProcessingInput();
  NDS_endProcessingInput();
  NDS_exec<false>();

  if (shouldDraw) {
    gpu_screen_to_rgb((u32 *)dstFrameBuffer);
  }

  return ret;
}

void AudioOut_Resample(s16 *inbuf, int inlen, s16 *outbuf, int outlen) {
  float res_incr = inlen / (float)outlen;
  float res_timer = 0;
  int res_pos = 0;
  int prev0 = inbuf[0], prev1 = inbuf[1];

  for (int i = 0; i < outlen; i++) {
    int curr0 = inbuf[res_pos * 2];
    int curr1 = inbuf[res_pos * 2 + 1];
    outbuf[i * 2] = ((curr0));
    outbuf[i * 2 + 1] = ((curr1));

    res_timer += res_incr;
    while (res_timer >= 1.0) {
      prev0 = curr0;
      prev1 = curr1;
      res_timer -= 1.0;
      res_pos++;
    }
  }
}

int fillAudioBuffer(int bufLenToFill) {
  samplesDesired = bufLenToFill;
  samplesRead = 0;
  SPU_Emulate_user();
  /*
  //printf("%d %d\n", samplesRead, bufLenToFill);
  if (samplesRead == bufLenToFill) {
    memcpy(audioBuffer, samplesBuffer, sizeof(s16) * 2 * bufLenToFill);
  } else {
    //printf("should not happen...\n");
    AudioOut_Resample(samplesBuffer, samplesRead, audioBuffer, bufLenToFill);
  }
*/
  return samplesRead;
}
}