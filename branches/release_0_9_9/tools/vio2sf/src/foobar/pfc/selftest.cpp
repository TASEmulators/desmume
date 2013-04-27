#include "pfc.h"

static void selftest() //never called, testing done at compile time
{
	pfc::static_assert_t<sizeof(t_uint8) == 1>();
	pfc::static_assert_t<sizeof(t_uint16) == 2>();
	pfc::static_assert_t<sizeof(t_uint32) == 4>();
	pfc::static_assert_t<sizeof(t_uint64) == 8>();

	pfc::static_assert_t<sizeof(t_int8) == 1>();
	pfc::static_assert_t<sizeof(t_int16) == 2>();
	pfc::static_assert_t<sizeof(t_int32) == 4>();
	pfc::static_assert_t<sizeof(t_int64) == 8>();

	pfc::static_assert_t<sizeof(t_float32) == 4>();
	pfc::static_assert_t<sizeof(t_float64) == 8>();

	pfc::static_assert_t<sizeof(t_size) == sizeof(void*)>();
	pfc::static_assert_t<sizeof(t_ssize) == sizeof(void*)>();

	pfc::static_assert_t<sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4>();

	pfc::static_assert_t<sizeof(GUID) == 16>();

	/*typedef pfc::avltree_t<int> t_asdf;
	t_asdf asdf; asdf.add_item(1);
	t_asdf::iterator iter = asdf._first_var();
	t_asdf::const_iterator iter2 = asdf._first_var();*/
}