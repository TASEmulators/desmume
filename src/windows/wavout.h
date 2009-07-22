bool DRV_WavBegin(const char* fname);
void DRV_WavEnd();
void DRV_WavSoundUpdate(void* soundData, int soundLen);
bool DRV_WavIsRecording();