#include "foobar2000.h"

#ifdef _WIN32
#include <ks.h>
#include <ksmedia.h>

#if 0
#define SPEAKER_FRONT_LEFT             0x1
#define SPEAKER_FRONT_RIGHT            0x2
#define SPEAKER_FRONT_CENTER           0x4
#define SPEAKER_LOW_FREQUENCY          0x8
#define SPEAKER_BACK_LEFT              0x10
#define SPEAKER_BACK_RIGHT             0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#define SPEAKER_BACK_CENTER            0x100
#define SPEAKER_SIDE_LEFT              0x200
#define SPEAKER_SIDE_RIGHT             0x400
#define SPEAKER_TOP_CENTER             0x800
#define SPEAKER_TOP_FRONT_LEFT         0x1000
#define SPEAKER_TOP_FRONT_CENTER       0x2000
#define SPEAKER_TOP_FRONT_RIGHT        0x4000
#define SPEAKER_TOP_BACK_LEFT          0x8000
#define SPEAKER_TOP_BACK_CENTER        0x10000
#define SPEAKER_TOP_BACK_RIGHT         0x20000
#endif

static struct {DWORD m_wfx; unsigned m_native; } const g_translation_table[] =
{
	{SPEAKER_FRONT_LEFT,			audio_chunk::channel_front_left},
	{SPEAKER_FRONT_RIGHT,			audio_chunk::channel_front_right},
	{SPEAKER_FRONT_CENTER,			audio_chunk::channel_front_center},
	{SPEAKER_LOW_FREQUENCY,			audio_chunk::channel_lfe},
	{SPEAKER_BACK_LEFT,				audio_chunk::channel_back_left},
	{SPEAKER_BACK_RIGHT,			audio_chunk::channel_back_right},
	{SPEAKER_FRONT_LEFT_OF_CENTER,	audio_chunk::channel_front_center_left},
	{SPEAKER_FRONT_RIGHT_OF_CENTER,	audio_chunk::channel_front_center_right},
	{SPEAKER_BACK_CENTER,			audio_chunk::channel_back_center},
	{SPEAKER_SIDE_LEFT,				audio_chunk::channel_side_left},
	{SPEAKER_SIDE_RIGHT,			audio_chunk::channel_side_right},
	{SPEAKER_TOP_CENTER,			audio_chunk::channel_top_center},
	{SPEAKER_TOP_FRONT_LEFT,		audio_chunk::channel_top_front_left},
	{SPEAKER_TOP_FRONT_CENTER,		audio_chunk::channel_top_front_center},
	{SPEAKER_TOP_FRONT_RIGHT,		audio_chunk::channel_top_front_right},
	{SPEAKER_TOP_BACK_LEFT,			audio_chunk::channel_top_back_left},
	{SPEAKER_TOP_BACK_CENTER,		audio_chunk::channel_top_back_center},
	{SPEAKER_TOP_BACK_RIGHT,		audio_chunk::channel_top_back_right},
};


DWORD audio_chunk::g_channel_config_to_wfx(unsigned p_config)
{
	DWORD ret = 0;
	unsigned n;
	for(n=0;n<tabsize(g_translation_table);n++)
	{
		if (p_config & g_translation_table[n].m_native) ret |= g_translation_table[n].m_wfx;
	}
	return ret;
}

unsigned audio_chunk::g_channel_config_from_wfx(DWORD p_wfx)
{
	unsigned ret = 0;
	unsigned n;
	for(n=0;n<tabsize(g_translation_table);n++)
	{
		if (p_wfx & g_translation_table[n].m_wfx) ret |= g_translation_table[n].m_native;
	}
	return ret;
}

#endif


static unsigned g_audio_channel_config_table[] = 
{
	0,
	audio_chunk::channel_config_mono,
	audio_chunk::channel_config_stereo,
	audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_lfe,
	audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_back_left | audio_chunk::channel_back_right,
	audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_back_left | audio_chunk::channel_back_right | audio_chunk::channel_lfe,
	audio_chunk::channel_config_5point1,
	audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_back_left | audio_chunk::channel_back_right | audio_chunk::channel_lfe | audio_chunk::channel_front_center_right | audio_chunk::channel_front_center_left,
	audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_back_left | audio_chunk::channel_back_right | audio_chunk::channel_front_center | audio_chunk::channel_lfe | audio_chunk::channel_front_center_right | audio_chunk::channel_front_center_left,
};


unsigned audio_chunk::g_guess_channel_config(unsigned count)
{
	if (count >= tabsize(g_audio_channel_config_table)) return 0;
	return g_audio_channel_config_table[count];
}


unsigned audio_chunk::g_channel_index_from_flag(unsigned p_config,unsigned p_flag) {
	unsigned index = 0;
	for(unsigned walk = 0; walk < 32; walk++) {
		unsigned query = 1 << walk;
		if (p_flag & query) return index;
		if (p_config & query) index++;
	}
	return infinite;
}

unsigned audio_chunk::g_extract_channel_flag(unsigned p_config,unsigned p_index)
{
	unsigned toskip = p_index;
	unsigned flag = 1;
	while(flag)
	{
		if (p_config & flag)
		{
			if (toskip == 0) break;
			toskip--;
		}
		flag <<= 1;
	}
	return flag;
}

unsigned audio_chunk::g_count_channels(unsigned p_config)
{
	unsigned ret = 0;
	while(p_config) {
		ret += (p_config & 1);
		p_config >>= 1;
	}
	return ret;
}
