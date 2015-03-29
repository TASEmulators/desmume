class NOVTABLE resampler_entry : public dsp_entry
{
public:
	virtual bool is_conversion_supported(unsigned p_srate_from,unsigned p_srate_to) = 0;
	virtual bool create_preset(dsp_preset & p_out,unsigned p_target_srate,float p_qualityscale) = 0;//p_qualityscale is 0...1
	virtual float get_priority() = 0;//value is 0...1, where high-quality (SSRC etc) has 1

	static bool g_get_interface(service_ptr_t<resampler_entry> & p_out,unsigned p_srate_from,unsigned p_srate_to);
	static bool g_create(service_ptr_t<dsp> & p_out,unsigned p_srate_from,unsigned p_srate_to,float p_qualityscale);
	static bool g_create_preset(dsp_preset & p_out,unsigned p_srate_from,unsigned p_srate_to,float p_qualityscale);

	FB2K_MAKE_SERVICE_INTERFACE(resampler_entry,dsp_entry);
};

template<typename T>
class resampler_entry_impl_t : public dsp_entry_impl_t<T,resampler_entry>
{
public:
	bool is_conversion_supported(unsigned p_srate_from,unsigned p_srate_to) {return T::g_is_conversion_supported(p_srate_from,p_srate_to);}
	bool create_preset(dsp_preset & p_out,unsigned p_target_srate,float p_qualityscale) {return T::g_create_preset(p_out,p_target_srate,p_qualityscale);}
	float get_priority() {return T::g_get_priority();}
};

template<typename T>
class resampler_factory_t : public service_factory_single_t<resampler_entry_impl_t<T> > {};
