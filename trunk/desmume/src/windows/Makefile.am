bin_PROGRAMS = desmume
desmume_SOURCES = \
	CWindow.cpp CWindow.hpp IORegView.cpp IORegView.hpp \
	des_view.cpp des_view.hpp ginfo.cpp ginfo.hpp \
	main.cpp mapView.cpp mapView.hpp mem_view.cpp mem_view.hpp \
	oamView.cpp oamView.hpp palView.cpp palView.hpp \
	resource.h tileView.cpp tileView.hpp yopyop_private.hpp
desmume_LDADD = ../libdesmume.a yopyop_private.o
yopyop_private.o: yopyop_private.rc resources.rc resource.h
	windres yopyop_private.rc -o yopyop_private.o
