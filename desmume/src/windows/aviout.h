bool DRV_AviBegin(const char* fname);
void DRV_AviEnd();
void DRV_AviSoundUpdate(void* soundData, int soundLen);
bool DRV_AviIsRecording();
void DRV_AviVideoUpdate(const u16* buffer);