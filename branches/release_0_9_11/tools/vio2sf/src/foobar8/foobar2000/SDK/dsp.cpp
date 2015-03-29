#include "foobar2000.h"
#include <math.h>

#if defined(_MSC_VER) && !defined(_DEBUG) && defined(_M_IX86)
#define DSP_HAVE_ASM
#endif

GUID dsp::guid_from_name(const char * name)
{
	service_enum_t<dsp> e;
	dsp * ptr;
	for(ptr=e.first();ptr;ptr = e.next())
	{
		if (!stricmp_utf8(ptr->get_name(),name))
		{
			GUID g = ptr->get_guid();
			ptr->service_release();
			return g;
		}
		ptr->service_release();
	}
	return mem_ops<GUID>::make_null_item();
}

const char * dsp::name_from_guid(GUID g)
{
	service_enum_t<dsp> e;
	dsp * ptr;
	for(ptr=e.first();ptr;ptr = e.next())
	{
		if (ptr->get_guid()==g)
		{
			const char * name = ptr->get_name();
			ptr->service_release();
			return name;
		}
		ptr->service_release();
	}
	return 0;
}

dsp * dsp::instance_from_guid(GUID g)
{
	service_enum_t<dsp> e;
	dsp * ptr;
	for(ptr=e.first();ptr;ptr = e.next())
	{
		if (ptr->get_guid()==g) return ptr;
		ptr->service_release();
	}
	return 0;
}

#ifndef DSP_HAVE_ASM
void dsp_util::scale(audio_sample * ptr,double scale,unsigned count)
{
	for(;count;count--)
	{
		(*ptr) = (audio_sample)((*ptr) * scale);
		ptr++;
	}
}
#else

__declspec(naked) void dsp_util::scale(audio_sample * ptr,double scale,unsigned count)
{
	_asm
	{//ptr: [esp+4], scale: [esp+8], count: [esp+16]
		mov ecx,[esp+16]
		mov edx,[esp+4]		
		fld qword ptr [esp+8]
		mov eax,ecx
		shr ecx,1
		jz l12
l1:
		fld audio_sample_asm ptr [edx]
		fld audio_sample_asm ptr [edx+audio_sample_bytes]
		fmul st,st(2)
		fxch
		dec ecx
		fmul st,st(2)
		fstp audio_sample_asm ptr [edx]
		fstp audio_sample_asm ptr [edx+audio_sample_bytes]
		lea edx,[edx+audio_sample_bytes*2]
		jnz l1
l12:
		test eax,1
		jz l2
		fld audio_sample_asm ptr [edx]
		fmul st,st(1)
		fstp audio_sample_asm ptr [edx]
l2:
		fstp st
		ret
	}
}
#endif

void dsp_util::kill_denormal_32(float * fptr,unsigned count)
{
	DWORD * ptr = reinterpret_cast<DWORD*>(fptr);
	for(;count;count--)
	{
		DWORD t = *ptr;
		if ((t & 0x007FFFFF) && !(t & 0x7F800000)) *ptr=0;
		ptr++;
	}
}

void dsp_util::kill_denormal_64(double * fptr,unsigned count)
{
	unsigned __int64 * ptr = reinterpret_cast<unsigned __int64*>(fptr);
	for(;count;count--)
	{
		unsigned __int64 t = *ptr;
		if ((t & 0x000FFFFFFFFFFFFF) && !(t & 0x7FF0000000000000)) *ptr=0;
		ptr++;
	}
}


unsigned dsp_chunk_list_i::get_count() const {return data.get_count();}

audio_chunk * dsp_chunk_list_i::get_item(unsigned n) const {return n>=0 && n<(unsigned)data.get_count() ? data[n] : 0;}

void dsp_chunk_list_i::remove_by_idx(unsigned idx)
{
	if (idx>=0 && idx<(unsigned)data.get_count())
		recycled.add_item(data.remove_by_idx(idx));
}

void dsp_chunk_list_i::remove_mask(const bit_array & mask)
{
	unsigned n, m = data.get_count();
	for(n=0;n<m;n++)
		if (mask[m])
			recycled.add_item(data[n]);
	data.remove_mask(mask);
}

audio_chunk * dsp_chunk_list_i::insert_item(unsigned idx,unsigned hint_size)
{
	unsigned max = get_count();
	if (idx<0) idx=0;
	else if (idx>max) idx = max;
	audio_chunk_i * ret = 0;
	if (recycled.get_count()>0)
	{
		unsigned best;
		if (hint_size>0)
		{
			best = 0;
			unsigned best_found = recycled[0]->get_data_size(), n, total = recycled.get_count();
			for(n=1;n<total;n++)
			{
				if (best_found==hint_size) break;
				unsigned size = recycled[n]->get_data_size();
				int delta_old = abs((int)best_found - (int)hint_size), delta_new = abs((int)size - (int)hint_size);
				if (delta_new < delta_old)
				{
					best_found = size;
					best = n;
				}
			}
		}
		else best = recycled.get_count()-1;

		ret = recycled.remove_by_idx(best);
		ret->set_sample_count(0);
		ret->set_channels(0);
		ret->set_srate(0);
	}
	else ret = new audio_chunk_i;
	if (idx==max) data.add_item(ret);
	else data.insert_item(ret,idx);
	return ret;
}

dsp_chunk_list_i::~dsp_chunk_list_i() {data.delete_all();recycled.delete_all();}

void dsp_chunk_list::remove_bad_chunks()
{
	bool blah = false;
	unsigned idx;
	for(idx=0;idx<get_count();)
	{
		audio_chunk * chunk = get_item(idx);
		if (!chunk->is_valid())
		{
			chunk->reset();
			remove_by_idx(idx);
			blah = true;
		}
		else idx++;
	}
	if (blah) console::error("one or more bad chunks removed from dsp chunk list");
}

__int64 dsp_util::duration_samples_from_time(double time,unsigned srate)
{
	return (__int64)floor((double)srate * time + 0.5);
}


#ifdef DSP_HAVE_ASM

__declspec(naked) void __fastcall dsp_util::convert_32_to_64(const float  * src,double * dest,unsigned count)
{
	_asm//src: ecx, dest: edx, count: eax
	{
		mov eax,dword ptr [esp+4]
		shr eax,2
		jz l2
l1:		fld dword ptr [ecx]
		fld dword ptr [ecx+4]
		fstp qword ptr [edx+8]
		fstp qword ptr [edx]
		dec eax
		fld dword ptr [ecx+8]
		fld dword ptr [ecx+12]
		fstp qword ptr [edx+24]
		fstp qword ptr [edx+16]
		lea ecx,[ecx+16]
		lea edx,[edx+32]
		jnz l1
l2:		mov eax,dword ptr [esp+4]
		and eax,3
		jz l4
l3:		fld dword ptr [ecx]
		dec eax
		fstp qword ptr [edx]
		lea ecx,[ecx+4]
		lea edx,[edx+8]
		jnz l3
l4:		ret 4
	}
}

__declspec(naked) void __fastcall dsp_util::convert_64_to_32(const double * src,float  * dest,unsigned count)
{
	_asm//src: ecx, dest: edx, count: eax
	{
		mov eax,dword ptr [esp+4]
		shr eax,2
		jz l2
l1:		fld qword ptr [ecx]
		fld qword ptr [ecx+8]
		fstp dword ptr [edx+4]
		fstp dword ptr [edx]
		dec eax
		fld qword ptr [ecx+16]
		fld qword ptr [ecx+24]
		fstp dword ptr [edx+12]
		fstp dword ptr [edx+8]
		lea ecx,[ecx+32]
		lea edx,[edx+16]
		jnz l1
l2:		mov eax,dword ptr [esp+4]
		and eax,3
		jz l4
l3:		fld qword ptr [ecx]
		dec eax
		fstp dword ptr [edx]
		lea ecx,[ecx+8]
		lea edx,[edx+4]
		jnz l3
l4:		ret 4
	}
}

#else

void __fastcall dsp_util::convert_32_to_64(const float  * src,double * dest,unsigned count)
{
	for(;count;count--)
		*(dest++) = (double)*(src++);
}

void __fastcall dsp_util::convert_64_to_32(const double * src,float  * dest,unsigned count)
{
	for(;count;count--)
		*(dest++) = (float)*(src++);
}

#endif


void dsp::get_config(cfg_var::write_config_callback * out)
{
	dsp_v2 * this_v2 = service_query_t(dsp_v2,this);
	if (this_v2)
	{
		this_v2->get_config(out);
		this_v2->service_release();
	}
}
void dsp::set_config(const void * src,unsigned bytes)
{
	dsp_v2 * this_v2 = service_query_t(dsp_v2,this);
	if (this_v2)
	{
		this_v2->set_config(src,bytes);
		this_v2->service_release();
	}
}
bool dsp::popup_config(HWND parent)
{
	bool rv = false;
	dsp_v2 * this_v2 = service_query_t(dsp_v2,this);
	if (this_v2)
	{
		rv = this_v2->popup_config(parent);
		this_v2->service_release();
	}
	return rv;
}
