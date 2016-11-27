#	Copyright (C) 2016 DeSmuME team
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

cd "${SRCROOT}/../../../"

REVISION=`git rev-parse HEAD`
if [ "$REVISION" == "" ] ; then
	REVISION="0"
fi

DESCRIBE=`git describe --always --long`
if [ "$DESCRIBE" == "" ] ; then
	DESCRIBE="0"
fi

BRANCH=`git rev-parse --abbrev-ref HEAD`
if [ "$BRANCH" == "" ] ; then
	BRANCH="unknown"
fi

ISSTABLE="0"
if [ "$BRANCH" = "master" ] || [ "$BRANCH" = "stable" ] ; then
	ISSTABLE="1"
fi

printf "// REVISION TRACKING\n\
// This file is auto-generated.\n\
// Do not commit this file to the code repository!\n\
#define SCR_REV_STR \"$REVISION\"\n\
#define SCM_DESC_STR \"$DESCRIBE\"\n\
#define SCM_BRANCH_STR \"$BRANCH\"\n\
#define SCM_IS_MASTER $ISSTABLE\n" > ./src/scmrev.h
