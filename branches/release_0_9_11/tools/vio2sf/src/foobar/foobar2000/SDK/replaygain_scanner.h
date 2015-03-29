//! Container of ReplayGain scan results from one or more tracks.
class replaygain_result : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE(replaygain_result, service_base);
public:
	//! Retrieves the gain value, in dB.
	virtual float get_gain() = 0;
	//! Retrieves the peak value, normalized to 0..1 range (audio_sample value).
	virtual float get_peak() = 0;
	//! Merges ReplayGain scan results from different tracks. Merge results from all tracks in an album to get album gain/peak values. \n
	//! This function returns a newly created replaygain_result object. Existing replaygain_result objects remain unaltered.
	virtual replaygain_result::ptr merge(replaygain_result::ptr other) = 0;
};

//! Instance of a ReplayGain scanner. \n
//! Use static_api_ptr_t<replaygain_scanner_entry>()->instantiate() to create a replaygain_scanner object; see replaygain_scanner_entry for more info. \n
//! Typical use: call process_chunk() with each chunk read from your track, call finalize() to obtain results for this track and reset replaygain_scanner's state for scanning another track; to obtain album gain/peak values, merge results (replaygain_result::merge) from all tracks. \n
class replaygain_scanner : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE(replaygain_scanner, service_base);
public:
	//! Processes a PCM chunk. \n
	//! May throw exception_io_data if the chunk contains something that can't be processed properly.
	virtual void process_chunk(const audio_chunk & chunk) = 0;
	//! Finalizes the scanning process; resets scanner's internal state and returns results for the track we've just scanned. \n
	//! After calling finalize(), scanner's state becomes the same as after instantiation; you can continue with processing another track without creating a new scanner object.
	virtual replaygain_result::ptr finalize() = 0;
};


//! Entrypoint class for instantiating replaygain_scanner objects. Use static_api_ptr_t<replaygain_scanner_entry>()->instantiate() to create replaygain_scanner instances. \n
//! This service is OPTIONAL; it's available from foobar2000 0.9.5.3 up but only if the ReplayGain Scanner component is installed. \n
//! It is recommended that you use replaygain_scanner like this: try { myInstance = static_api_ptr_t<replaygain_scanner_entry>()->instantiate(); } catch(exception_service_not_found) { /* too old foobar2000 version or no foo_rgscan installed - complain/fail/etc */ }
class replaygain_scanner_entry : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(replaygain_scanner_entry);
public:
	//! Instantiates a replaygain_scanner object.
	virtual replaygain_scanner::ptr instantiate() = 0;
};
