(*
	Copyright (C) 2013 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
 *)

display dialog "Are you sure that you want to delete DeSmuME's preferences?" buttons {"Cancel", "Delete"} default button "Delete" cancel button "Cancel" with title "Delete DeSmuME Preferences"

try
	set userDefaultsPath to ((path to "pref" as text) & "org.desmume.DeSmuME.plist")
	set isUserDefaultsDeleted to false
	
	tell application "Finder"
		if exists file userDefaultsPath then
			set isUserDefaultsDeleted to true
			delete file userDefaultsPath
		end if
		
		if isUserDefaultsDeleted then
			display dialog "The preference files have been moved to the Trash." buttons {"OK"} default button {"OK"}
		else
			display dialog "No preference files were found." buttons {"OK"} default button {"OK"}
		end if
	end tell
	
on error errorMsg
	display dialog errorMsg
end try