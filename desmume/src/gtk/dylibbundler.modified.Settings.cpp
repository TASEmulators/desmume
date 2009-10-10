/*
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "Settings.h"
#include <vector>
#include <iostream>

namespace Settings
{

bool overwrite_files = false;
bool overwrite_dir = false;
bool create_dir = false;

bool canOverwriteFiles(){ return overwrite_files; }
bool canOverwriteDir(){ return overwrite_dir; }
bool canCreateDir(){ return create_dir; }

void canOverwriteFiles(bool permission){ overwrite_files = permission; }
void canOverwriteDir(bool permission){ overwrite_dir = permission; }
void canCreateDir(bool permission){ create_dir = permission; }


bool bundleLibs_bool = false;
bool bundleLibs(){ return bundleLibs_bool; }
void bundleLibs(bool on){ bundleLibs_bool = on; }


std::string dest_folder_str = "./libs/";
std::string destFolder(){ return dest_folder_str; }
void destFolder(std::string path)
{
    dest_folder_str = path;
    // fix path if needed so it ends with '/'
	if( dest_folder_str[ dest_folder_str.size()-1 ] != '/' ) dest_folder_str += "/";
}

std::vector<std::string> files;
void addFileToFix(std::string path){ files.push_back(path); }
int fileToFixAmount(){ return files.size(); }
std::string fileToFix(const int n){ return files[n]; }

std::string inside_path_str = "@executable_path/../libs/";
std::string inside_lib_path(){ return inside_path_str; }
void inside_lib_path(std::string p)
{
    inside_path_str = p;
    // fix path if needed so it ends with '/'
	if( inside_path_str[ inside_path_str.size()-1 ] != '/' ) inside_path_str += "/";
}

std::vector<std::string> prefixes_to_ignore;
void ignore_prefix(std::string prefix)
{
    if( prefix[ prefix.size()-1 ] != '/' ) prefix += "/";
    prefixes_to_ignore.push_back(prefix);
}

bool isPrefixBundled(std::string prefix)
{
    //std::cout << "Testing " << prefix << " : " << std::endl;
    
    if(prefix.find("Gtk.framework") == std::string::npos
    && prefix.find("GLib.framework") == std::string::npos
    && prefix.find("Cairo.framework") == std::string::npos)
      if(prefix.find(".framework") != std::string::npos) return false;
    if(prefix.find("@executable_path") != std::string::npos) return false;
    if(prefix.compare("/usr/lib/") == 0) return false;
    
    const int prefix_amount = prefixes_to_ignore.size();
    for(int n=0; n<prefix_amount; n++)
    {
        if(prefix.compare(prefixes_to_ignore[n]) == 0) return false;
    }

   //printf("TRUE!\n");
    
    return true;
}

}
