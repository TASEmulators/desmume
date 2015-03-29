#ifndef _INPUT_HELPERS_H_
#define _INPUT_HELPERS_H_

#include "input.h"

class NOVTABLE input_pcm : public input
{
public:
	virtual int run(audio_chunk * chunk);
protected:
	virtual int get_samples_pcm(void ** buffer,int * size,int * srate,int * bps,int * nch)=0;//same return value as input::run()
};

class input_helper//takes care of opening inputs etc
{
private:
	critical_section sync;
	playable_location_i my_location;
	input * p_input;
	reader * p_reader;
	__int64 full_buffer_limit;
	bool b_aborting;
	bool reader_overridden;
	bool b_no_seek,b_no_loop,b_want_info;
	double seek_to;

	bool open_file_internal(const char * path,reader * p_reader_override);
	bool attempt_reopen_internal(file_info * info,reader * p_reader_override,__int64 * modtime,__int64* filesize,bool * new_info);
	bool open_internal(file_info * info,reader * p_reader_override,__int64 * modtime,__int64* filesize,bool * new_info);
	void close_input_internal();
	void close_reader_internal();
	void override_reader(reader*);
	void setup_fullbuffer();
	unsigned get_open_flags();
public:
	input_helper();
	~input_helper();
	bool open(file_info * info,reader * p_reader_override = 0);
	bool open_noinfo(file_info * info, reader * p_reader_override=0);
	bool open(const playable_location * src,reader * p_reader_override = 0);//helper
	inline bool open(const char * path,reader * p_reader_override = 0)
	{return open(make_playable_location(path,0),p_reader_override);}

	bool open(class metadb_handle * p_handle, reader * p_reader_override=0);
	
	


	void override(const playable_location * new_location,input * new_input,reader * new_reader);

	void close();
	bool is_open();
	int run(audio_chunk * chunk);
	bool seek(double seconds);
	bool can_seek();
	void abort();
	void set_full_buffer(__int64 val) {full_buffer_limit = val;}//default 0 to disable, any positive value to set upper file size limit for full file buffering
	void on_idle();
	bool get_dynamic_info(file_info * out,double *timestamp_delta,bool *b_track_change);

	//call these before opening
	inline void hint_no_seeking(bool state) {b_no_seek = state;}
	inline void hint_no_looping(bool state) {b_no_loop = state;}

	bool open_errorhandled(class metadb_handle * p_handle, reader * p_reader_override=0);
	int run_errorhandled(audio_chunk * chunk);
	bool seek_errorhandled(double seconds);
	void close_errorhandled();
};

class input_test_filename_helper//avoids bloody bottleneck with creating inputs when mass-testing filenames
{
	ptr_list_t<input> inputs;
	bool inited;
public:
	inline input_test_filename_helper() {inited=false;}

	bool test_filename(const char * filename);
	~input_test_filename_helper();
};

#endif