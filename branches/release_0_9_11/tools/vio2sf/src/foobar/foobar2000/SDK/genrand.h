//! PRNG service. Implemented by the core, do not reimplement.  Use g_create() helper function to instantiate.
class NOVTABLE genrand_service : public service_base
{
public:
	//! Seeds the PRNG with specified value.
	virtual void seed(unsigned val) = 0;
	//! Returns random value N, where 0 <= N < range.
	virtual unsigned genrand(unsigned range)=0;

	static service_ptr_t<genrand_service> g_create() {return standard_api_create_t<genrand_service>();}

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(genrand_service);
};
