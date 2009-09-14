#include "pcre_internal.h"

/*
 * This function is not needed by GRegex, so print an error and
 * return always -1, that is the string is a valid UTF-8 encoded
 * string.
 */
int
_pcre_valid_utf8(const uschar *string, int length)
{
g_warning ("%s: this function should not be called", G_STRLOC);
return -1;
}
