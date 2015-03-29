First build every project in vio2sf.dsw. This will include the vio2sf project which will copy vio2sf.bin into all the plugin dirs.
Then build foo_input_vio2sf from vio2sf.sln. This plugin my be built in a newer msvc. 
It has a dependency on vio2sf, which will then overwrite all vio2sf.bin in all directories with a vc9 version.
This is safe since it doesn't import any CRT DLL, and it probably runs a little faster.
