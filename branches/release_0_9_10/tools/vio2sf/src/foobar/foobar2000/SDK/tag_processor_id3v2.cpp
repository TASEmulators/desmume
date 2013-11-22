#include "foobar2000.h"

bool tag_processor_id3v2::g_get(service_ptr_t<tag_processor_id3v2> & p_out)
{
	return service_enum_t<tag_processor_id3v2>().first(p_out);
}

void tag_processor_id3v2::g_remove(const service_ptr_t<file> & p_file,t_uint64 & p_size_removed,abort_callback & p_abort) {
	g_remove_ex(tag_write_callback_dummy(),p_file,p_size_removed,p_abort);
}

void tag_processor_id3v2::g_remove_ex(tag_write_callback & p_callback,const service_ptr_t<file> & p_file,t_uint64 & p_size_removed,abort_callback & p_abort)
{
	p_file->ensure_seekable();

	t_filesize len;
	
	len = p_file->get_size(p_abort);

	if (len == filesize_invalid) throw exception_io_no_length();
	
	p_file->seek(0,p_abort);
	
	t_uint64 offset;
	g_skip(p_file,offset,p_abort);
	
	if (offset>0 && offset<len)
	{
		len-=offset;
		service_ptr_t<file> temp;
		if (p_callback.open_temp_file(temp,p_abort)) {
			file::g_transfer_object(p_file,temp,len,p_abort);
		} else {
			if (len > 16*1024*1024) filesystem::g_open_temp(temp,p_abort);
			else filesystem::g_open_tempmem(temp,p_abort);
			file::g_transfer_object(p_file,temp,len,p_abort);
			p_file->seek(0,p_abort);
			p_file->set_eof(p_abort);
			temp->seek(0,p_abort);
			file::g_transfer_object(temp,p_file,len,p_abort);
		}
	}
	p_size_removed = offset;
}

void tag_processor_id3v2::g_skip(const service_ptr_t<file> & p_file,t_uint64 & p_size_skipped,abort_callback & p_abort)
{

	unsigned char  tmp[10];

	t_size io_bytes_done;

	p_file->seek ( 0, p_abort );

	io_bytes_done = p_file->read( tmp, sizeof(tmp),  p_abort);
	if (io_bytes_done != sizeof(tmp))  {
		p_file->seek ( 0, p_abort );
		p_size_skipped = 0;
		return;
	}

	if ( 0 != memcmp ( tmp, "ID3", 3) ) {
		p_file->seek ( 0, p_abort );
		p_size_skipped = 0;
		return;
	}

	int Unsynchronisation = tmp[5] & 0x80;
	int ExtHeaderPresent  = tmp[5] & 0x40;
	int ExperimentalFlag  = tmp[5] & 0x20;
	int FooterPresent     = tmp[5] & 0x10;

	if ( tmp[5] & 0x0F ) {
		p_file->seek ( 0, p_abort );
		p_size_skipped = 0;
		return;
	}

	if ( (tmp[6] | tmp[7] | tmp[8] | tmp[9]) & 0x80 ) {
		p_file->seek ( 0, p_abort );
		p_size_skipped = 0;
		return;
	}

	t_uint32 ret;
	ret  = tmp[6] << 21;
	ret += tmp[7] << 14;
	ret += tmp[8] <<  7;
	ret += tmp[9]      ;
	ret += 10;
	if ( FooterPresent ) ret += 10;

	p_file->seek ( ret, p_abort );

	p_size_skipped = ret;

}
