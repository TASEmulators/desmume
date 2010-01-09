class NOVTABLE core_version_info : public service_base {
public:
	virtual const char * get_version_string() = 0;
	static const char * g_get_version_string() {return static_api_ptr_t<core_version_info>()->get_version_string();}
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(core_version_info);
};

struct t_core_version_data {
	t_uint32 m_major, m_minor1, m_minor2, m_minor3;
};

//! New (0.9.4.2)
class NOVTABLE core_version_info_v2 : public core_version_info {
public:
	virtual const char * get_name() = 0;//"foobar2000"
	virtual const char * get_version_as_text() = 0;//"N.N.N.N"
	virtual t_core_version_data get_version() = 0;

	//! Determine whether running foobar2000 version is newer or equal to the specified version, eg. test_version(0,9,5,0) for 0.9.5.
	bool test_version(t_uint32 major, t_uint32 minor1, t_uint32 minor2, t_uint32 minor3) {
		const t_core_version_data v = get_version();
		if (v.m_major < major) return false;
		else if (v.m_major > major) return true;
		// major version matches
		else if (v.m_minor1 < minor1) return false;
		else if (v.m_minor1 > minor1) return true;
		// minor1 version matches
		else if (v.m_minor2 < minor2) return false;
		else if (v.m_minor2 > minor2) return true;
		// minor2 version matches
		else if (v.m_minor3 < minor3) return false;
		else return true;
	}

	FB2K_MAKE_SERVICE_INTERFACE(core_version_info_v2, core_version_info);
};
