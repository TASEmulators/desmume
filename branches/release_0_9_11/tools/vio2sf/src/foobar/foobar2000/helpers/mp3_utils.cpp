#include "stdafx.h"

using namespace bitreader_helper;

static unsigned extract_header_bits(const t_uint8 p_header[4],unsigned p_base,unsigned p_bits)
{
	assert(p_base+p_bits<=32);
	return extract_bits(p_header,p_base,p_bits);
}

namespace {

	class header_parser
	{
	public:
		header_parser(const t_uint8 p_header[4]) : m_bitptr(0) 
		{
			memcpy(m_header,p_header,4);
		}
		unsigned read(unsigned p_bits)
		{
			unsigned ret = extract_header_bits(m_header,m_bitptr,p_bits);
			m_bitptr += p_bits;
			return ret;
		}
	private:
		t_uint8 m_header[4];
		unsigned m_bitptr;
	};
}

typedef t_uint16 uint16;

static const uint16 bitrate_table_l1v1[16]  = {  0, 32, 64, 96,128,160,192,224,256,288,320,352,384,416,448,  0};
static const uint16 bitrate_table_l2v1[16]  = {  0, 32, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,384,  0};
static const uint16 bitrate_table_l3v1[16]  = {  0, 32, 40, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,  0};
static const uint16 bitrate_table_l1v2[16]  = {  0, 32, 48, 56, 64, 80, 96,112,128,144,160,176,192,224,256,  0};
static const uint16 bitrate_table_l23v2[16] = {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160,  0};
static const uint16 sample_rate_table[] = {11025,12000,8000};

unsigned mp3_utils::QueryMPEGFrameSize(const t_uint8 p_header[4])
{
	TMPEGFrameInfo info;
	if (!ParseMPEGFrameHeader(info,p_header)) return 0;
	return info.m_bytes;
}

bool mp3_utils::ParseMPEGFrameHeader(TMPEGFrameInfo & p_info,const t_uint8 p_header[4])
{
	enum {MPEG_LAYER_1 = 3, MPEG_LAYER_2 = 2, MPEG_LAYER_3 = 1};
	enum {_MPEG_1 = 3, _MPEG_2 = 2, _MPEG_25 = 0};

	header_parser parser(p_header);
	if (parser.read(11) != 0x7FF) return false;
	unsigned mpeg_version = parser.read(2);
	unsigned layer = parser.read(2);
	unsigned protection = parser.read(1);
	unsigned bitrate_index = parser.read(4);
	unsigned sample_rate_index = parser.read(2);
	if (sample_rate_index == 11) return false;//reserved
	unsigned paddingbit = parser.read(1);
	int paddingdelta = 0;
	parser.read(1);//private
	unsigned channel_mode = parser.read(2);
	parser.read(2);//channel_mode_extension
	parser.read(1);//copyright
	parser.read(1);//original
	parser.read(2);//emphasis

	unsigned bitrate = 0,sample_rate = 0;

	switch(layer)
	{
	default:
		return false;
	case MPEG_LAYER_3:
		paddingdelta = paddingbit ? 1 : 0;
		
		p_info.m_layer = 3;
		switch(mpeg_version)
		{
		case _MPEG_1:
			p_info.m_duration = 1152;
			bitrate = bitrate_table_l3v1[bitrate_index];
			break;
		case _MPEG_2:
		case _MPEG_25:
			p_info.m_duration = 576;
			bitrate = bitrate_table_l23v2[bitrate_index];
			break;
		default:
			return false;
		}
	
		break;
	case MPEG_LAYER_2:
		paddingdelta = paddingbit ? 1 : 0;
		p_info.m_duration = 1152;
		p_info.m_layer = 2;
		switch(mpeg_version)
		{
		case _MPEG_1:
			bitrate = bitrate_table_l2v1[bitrate_index];
			break;
		case _MPEG_2:
		case _MPEG_25:
			bitrate = bitrate_table_l23v2[bitrate_index];
			break;
		default:
			return false;
		}
		break;
	case MPEG_LAYER_1:
		paddingdelta = paddingbit ? 4 : 0;
		p_info.m_duration = 384;
		p_info.m_layer = 1;
		switch(mpeg_version)
		{
		case _MPEG_1:
			bitrate = bitrate_table_l1v1[bitrate_index];
			break;
		case _MPEG_2:
		case _MPEG_25:
			bitrate = bitrate_table_l1v2[bitrate_index];
			break;		
		default:
			return false;
		}
		break;
	}
	if (bitrate == 0) return false;

	sample_rate = sample_rate_table[sample_rate_index];
	if (sample_rate == 0) return false;
	switch(mpeg_version)
	{
	case _MPEG_1:
		sample_rate *= 4;
		p_info.m_mpegversion = MPEG_1;
		break;
	case _MPEG_2:
		sample_rate *= 2;
		p_info.m_mpegversion = MPEG_2;
		break;
	case _MPEG_25:
		p_info.m_mpegversion = MPEG_25;
		break;
	}

	switch(channel_mode)
	{
	case 0:
	case 1:
	case 2:
		p_info.m_channels = 2;
		break;
	case 3:
		p_info.m_channels = 1;
		break;
	}

	p_info.m_channel_mode = channel_mode;

	p_info.m_sample_rate = sample_rate;


	p_info.m_bytes = ( bitrate /*kbps*/ * (1000/8) /* kbps-to-bytes*/ * p_info.m_duration /*samples-per-frame*/ ) / sample_rate + paddingdelta;

	if (p_info.m_layer == 1) p_info.m_bytes &= ~3;

	p_info.m_crc = protection == 0;

	return true;
}

unsigned mp3header::get_samples_per_frame()
{
	mp3_utils::TMPEGFrameInfo fr;
	if (!decode(fr)) return 0;
	return fr.m_duration;
}

bool mp3_utils::IsSameStream(TMPEGFrameInfo const & p_frame1,TMPEGFrameInfo const & p_frame2) {
	return 
		p_frame1.m_channel_mode == p_frame2.m_channel_mode && 
		p_frame1.m_sample_rate == p_frame2.m_sample_rate &&
		p_frame1.m_layer == p_frame2.m_layer &&
		p_frame1.m_mpegversion == p_frame2.m_mpegversion;
}



bool mp3_utils::ValidateFrameCRC(const t_uint8 * frameData, t_size frameSize, TMPEGFrameInfo const & info) {
	if (frameSize < info.m_bytes) return false; //FAIL, incomplete data
	if (!info.m_crc) return true; //nothing to check, frame appears valid
	return ExtractFrameCRC(frameData, frameSize, info) == CalculateFrameCRC(frameData, frameSize, info);
}

static t_uint32 CRC_update(unsigned value, t_uint32 crc)
{
	enum { CRC16_POLYNOMIAL = 0x8005 };
    unsigned i;
    value <<= 8;
    for (i = 0; i < 8; i++) {
		value <<= 1;
		crc <<= 1;
		if (((crc ^ value) & 0x10000)) crc ^= CRC16_POLYNOMIAL;
    }
    return crc;
}


void mp3_utils::RecalculateFrameCRC(t_uint8 * frameData, t_size frameSize, TMPEGFrameInfo const & info) {
	PFC_ASSERT( frameSize >= info.m_bytes && info.m_crc );

	const t_uint16 crc = ExtractFrameCRC(frameData, frameSize, info);
	frameData[4] = (t_uint8)(crc >> 8);
	frameData[5] = (t_uint8)(crc & 0xFF);
}

static t_uint16 grabFrameCRC(const t_uint8 * frameData, t_size sideInfoLen) {
    t_uint32 crc = 0xffff;
    crc = CRC_update(frameData[2], crc);
    crc = CRC_update(frameData[3], crc);
    for (t_size i = 6; i < sideInfoLen; i++) {
		crc = CRC_update(frameData[i], crc);
    }

	return (t_uint32) (crc & 0xFFFF);
}

t_uint16 mp3_utils::ExtractFrameCRC(const t_uint8 * frameData, t_size frameSize, TMPEGFrameInfo const & info) {
	PFC_ASSERT( frameSize >= info.m_bytes && info.m_crc );

	return ((t_uint16)frameData[4] << 8) | (t_uint16)frameData[5];

}
t_uint16 mp3_utils::CalculateFrameCRC(const t_uint8 * frameData, t_size frameSize, TMPEGFrameInfo const & info) {
	PFC_ASSERT( frameSize >= info.m_bytes && info.m_crc );

	t_size sideInfoLen = 0;
	if (info.m_mpegversion == MPEG_1)
		sideInfoLen = (info.m_channels == 1) ? 4 + 17 : 4 + 32;
	else
		sideInfoLen = (info.m_channels == 1) ? 4 + 9 : 4 + 17;

	//CRC
	sideInfoLen += 2;

	PFC_ASSERT( sideInfoLen  <= frameSize );

	return grabFrameCRC(frameData, sideInfoLen);
}


bool mp3_utils::ValidateFrameCRC(const t_uint8 * frameData, t_size frameSize) {
	if (frameSize < 4) return false; //FAIL, not a valid frame
	TMPEGFrameInfo info;
	if (!ParseMPEGFrameHeader(info, frameData)) return false; //FAIL, not a valid frame
	return ValidateFrameCRC(frameData, frameSize, info);
}
