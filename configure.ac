AC_INIT(executor.hpp, 0.1-dev)

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([aux-bin])

AM_INIT_AUTOMAKE([foreign])

AM_SILENT_RULES([yes])

AC_PROG_CXX
AX_CXX_COMPILE_STDCXX(11)

DX_INIT_DOXYGEN(executor.hpp, [Doxyfile], [doxygen])

AC_OUTPUT([
Makefile
doc/Makefile
src/Makefile
samples/Makefile
tests/Makefile
])
