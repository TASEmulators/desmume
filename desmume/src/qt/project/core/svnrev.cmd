@echo off
rem Copyright (C) 2013-2014 DeSmuME team
rem
rem This file is free software: you can redistribute it and/or modify
rem it under the terms of the GNU General Public License as published by
rem the Free Software Foundation, either version 2 of the License, or
rem (at your option) any later version.
rem
rem This file is distributed in the hope that it will be useful
rem but WITHOUT ANY WARRANTY; without even the implied warranty of
rem MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
rem GNU General Public License for more details.
rem
rem You should have received a copy of the GNU General Public License
rem along with the this software.  If not, see <http://www.gnu.org/licenses/>.

echo Generating svnrev.h...
..\..\..\windows\defaultconfig\SubWCRev.exe ..\..\..\.. ".\defaultconfig\svnrev_template.h" ".\userconfig\svnrev.h"
exit 0
