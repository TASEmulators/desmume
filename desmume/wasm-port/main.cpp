
#include "NDSSystem.h"
#include "rasterize.h"
#include "SPU.h"
#include "../src/utils/colorspacehandler/colorspacehandler.h"
#include <emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

u8* romBuffer;
int romBufferCap = 0;
int romLen;
volatile bool execute = true;
volatile bool paused = false;
SoundInterface_struct *SNDCoreList[] = {
	&SNDDummy,
	NULL
};


GPU3DInterface *core3DList[] = {
        &gpu3DNull,
        &gpu3DRasterize,
        NULL
};


extern "C" {

s16 audioBuffer[16384 * 2];
u32 dstFrameBuffer[2][256 * 192];

static void gpu_screen_to_rgb(u32* dst)
{
    ColorspaceConvertBuffer555To8888Opaque<false, false>((const uint16_t *)GPU->GetDisplayInfo().masterNativeBuffer, dst, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
}

unsigned cpu_features_get_core_amount(void) {
  return 1;
}

int main() {
  NDS_Init();
  SPU_SetSynchMode(0, 0);
  SPU_SetVolume(100);

  GPU->Change3DRendererByID(RENDERID_SOFTRASTERIZER);
  printf("wasm ready.\n");
  EM_ASM(wasmReady(););

}

void* prepareRomBuffer(int rl) {
  romLen = rl;
  if (romLen > romBufferCap) {
    romBuffer = (u8*)realloc((void*)romBuffer, romLen);
    romBufferCap = romLen;
  }
  return romBuffer;
}

void* getSymbol(int id) {
  if (id == 0) {
    //return NDS::ARM7BIOS;
  }
  if (id == 1) {
    //return NDS::ARM9BIOS;
  }
  if (id == 2) {
    //return SPI_Firmware::Firmware;
  }
  if (id == 3) {
    //return NDSCart::CartROM;
  }
  if (id == 4) {
    return dstFrameBuffer;
  }

  if (id == 6) {
    return audioBuffer;
  }
  return 0;
}

void reset() {
  NDS_Reset();
}

int loadROM(int romLen) {
  return NDS_LoadROM("rom.nds");
}

static inline void convertFB(u8* dst, u8* src) {
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

int runFrame(int shouldDraw, u32 keys, int touched, u32 touchX, u32 touchY) {
  if (!shouldDraw) {
    NDS_SkipNextFrame();
  }
  int ret = 0;
  NDS_beginProcessingInput();
  NDS_endProcessingInput();
  NDS_exec<false>();
  SPU_Emulate_user();
  gpu_screen_to_rgb((u32*)dstFrameBuffer);
  return ret;
}

void AudioOut_Resample(s16* inbuf, int inlen, s16* outbuf, int outlen) {
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

int fillAudioBuffer(int bufLenToFill, int origSamplesDesired) {
  static s16 samplesBuffer[16384 * 2];

  int samplesRead = 0;//SPU::ReadOutput(samplesBuffer, origSamplesDesired);
  AudioOut_Resample(samplesBuffer, samplesRead, audioBuffer, bufLenToFill);
  return samplesRead;
}
}