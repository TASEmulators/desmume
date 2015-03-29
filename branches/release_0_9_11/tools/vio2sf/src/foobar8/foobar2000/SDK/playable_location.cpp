#include "foobar2000.h"

int playable_location::compare(const playable_location * src) const
{
	int ret = uStringCompare(get_path(),src->get_path());
	if (ret!=0) return ret;
	else
	{
		int n1 = get_number(), n2 = src->get_number();
		if (n1<n2) return -1;
		else if (n1>n2) return 1;
		else return 0;
	}
}
