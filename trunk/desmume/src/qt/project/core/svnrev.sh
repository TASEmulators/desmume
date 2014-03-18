#!/bin/sh
#	Copyright (C) 2013-2014 DeSmuME team
#
#	This file is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 2 of the License, or
#	(at your option) any later version.
#
#	This file is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with the this software.  If not, see <http://www.gnu.org/licenses/>.

echo "Obtaining svn revision..."

if which svnversion > /dev/null; then
	WC_PATH=""
	if [ -d "../../../../.svn" ]; then
		WC_PATH="../../../../"
	elif [ -d "../../../../../.svn" ]; then
		WC_PATH="../../../../../"
	fi
	REV=$(svnversion -n $WC_PATH | sed -e 's/[[:alpha:]]//g' -e 's/^[[:digit:]]*://')
fi
if [ "$REV" = "" ]; then
	echo "Cannot obtain svn revision."
	REV="0"
else
	echo "Obtained svn revision is $REV."
fi

sed 's/\$WCREV\$/'$REV'/' defaultconfig/svnrev_template.h > userconfig/svnrev_gen.h

if which diff > /dev/null && diff -q userconfig/svnrev_gen.h userconfig/svnrev.h > /dev/null 2>&1; then
	# Hopefully don't trigger a rebuild
	echo "Generated svnrev.h is the same as the existing one. Keeping original."
	rm userconfig/svnrev_gen.h
else
	echo "Updating svnrev.h..."
	rm userconfig/svnrev.h
	mv userconfig/svnrev_gen.h userconfig/svnrev.h
fi

