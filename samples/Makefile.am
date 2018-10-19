
AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"

AM_CFLAGS =\
	 -Wall\
	 -g

AM_CXXFLAGS = -std=c++11 -I../src
AM_LDFLAGS = -pthread

noinst_PROGRAMS = thread_exec tpool_sched

thread_exec_SOURCES = thread_exec.cpp

tpool_sched_SOURCES = tpool_sched.cpp
