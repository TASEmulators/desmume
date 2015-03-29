#	Copyright (C) 2013 DeSmuME team
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

cd "${SRCROOT}/../../"

REV=`svnversion -n | sed -e 's/[[:alpha:]]//g' -e 's/^[[:digit:]]*://'`
if test "$REV" == "" ; then
	REV="0"
fi

printf "// REVISION TRACKING\n\
// This file is auto-generated.\n\
// Do not commit this file to the code repository!\n\
#define SVN_REV $REV\n\
#define SVN_REV_STR \"$REV\"\n" > ./src/svnrev.h