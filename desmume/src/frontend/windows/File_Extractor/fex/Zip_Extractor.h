// ZIP archive extractor. Only supports deflation and store (no compression).

// File_Extractor 1.0.0
#ifndef ZIP_EXTRACTOR_H
#define ZIP_EXTRACTOR_H

#include "File_Extractor.h"
#include "Zlib_Inflater.h"

class Zip_Extractor : public File_Extractor {
public:
	Zip_Extractor();
	virtual ~Zip_Extractor();

protected:
	virtual blargg_err_t open_path_v();
	virtual blargg_err_t open_v();
	virtual void         close_v();
	
	virtual void         clear_file_v();
	virtual blargg_err_t next_v();
	virtual blargg_err_t rewind_v();
	virtual fex_pos_t    tell_arc_v() const;
	virtual blargg_err_t seek_arc_v( fex_pos_t );
	
	virtual blargg_err_t extract_v( void*, long );

private:
	blargg_vector<char> catalog;
    BOOST::int64_t catalog_begin;  // offset of first catalog entry in file (to detect corruption)
    BOOST::int64_t catalog_pos;    // position of current entry in catalog
	BOOST::uint64_t raw_remain;     // bytes remaining to be read from zip file for current file
	unsigned long crc;       // ongoing CRC of extracted bytes
	unsigned long correct_crc;
	bool file_deflated;
	Zlib_Inflater buf;

	blargg_err_t fill_buf( long offset, long buf_size, long initial_read );
	blargg_err_t update_info( bool advance_first );
	blargg_err_t first_read( long count );
	void reorder_entry_header( long offset );
	static blargg_err_t inflater_read( void* data, void* out, long* count );
};

#endif
