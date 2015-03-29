namespace mp3_utils
{

	enum {
		MPG_MD_STEREO=0,
		MPG_MD_JOINT_STEREO=1,
		MPG_MD_DUAL_CHANNEL=2,
		MPG_MD_MONO=3,
	};

	typedef t_uint8 byte;

	enum {MPEG_1, MPEG_2, MPEG_25};

	struct TMPEGFrameInfo
	{
		unsigned m_bytes;
		unsigned m_sample_rate;
		unsigned m_layer;
		unsigned m_mpegversion;
		unsigned m_channels;
		unsigned m_duration;
		unsigned m_channel_mode;
		bool m_crc;
	};


	bool ParseMPEGFrameHeader(TMPEGFrameInfo & p_info,const t_uint8 p_header[4]);
	bool ValidateFrameCRC(const t_uint8 * frameData, t_size frameSize);
	bool ValidateFrameCRC(const t_uint8 * frameData, t_size frameSize, TMPEGFrameInfo const & frameInfo);
	
	//! Assumes valid frame with CRC (frameInfo.m_crc set, frameInfo.m_bytes <= frameSize).
	t_uint16 ExtractFrameCRC(const t_uint8 * frameData, t_size frameSize, TMPEGFrameInfo const & frameInfo);
	//! Assumes valid frame with CRC (frameInfo.m_crc set, frameInfo.m_bytes <= frameSize).
	t_uint16 CalculateFrameCRC(const t_uint8 * frameData, t_size frameSize, TMPEGFrameInfo const & frameInfo);
	//! Assumes valid frame with CRC (frameInfo.m_crc set, frameInfo.m_bytes <= frameSize).
	void RecalculateFrameCRC(t_uint8 * frameData, t_size frameSize, TMPEGFrameInfo const & frameInfo);

	unsigned QueryMPEGFrameSize(const t_uint8 p_header[4]);
	bool IsSameStream(TMPEGFrameInfo const & p_frame1,TMPEGFrameInfo const & p_frame2);
};

class mp3header
{
	t_uint8 bytes[4];
public:

	inline void copy(const mp3header & src) {memcpy(bytes,src.bytes,4);}
	inline void copy_raw(const void * src) {memcpy(bytes,src,4);}

	inline mp3header(const mp3header & src) {copy(src);}
	inline mp3header() {}

	inline const mp3header & operator=(const mp3header & src) {copy(src); return *this;}

	inline void get_bytes(void * out) {memcpy(out,bytes,4);}
	inline unsigned get_frame_size() const {return mp3_utils::QueryMPEGFrameSize(bytes);}
	inline bool decode(mp3_utils::TMPEGFrameInfo & p_out) {return mp3_utils::ParseMPEGFrameHeader(p_out,bytes);}

	unsigned get_samples_per_frame();
};

static inline mp3header mp3header_from_buffer(const void * p_buffer)
{
	mp3header temp;
	temp.copy_raw(p_buffer);
	return temp;
}