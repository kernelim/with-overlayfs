AC_PREREQ([2.59])

AC_INIT([with-overlayfs], [0.1], [https://github.com/kernelim/with-overlayfs/issues], [with-overlayfs], [https://github.com/kernelim/with-overlayfs])
AM_INIT_AUTOMAKE([no-define foreign subdir-objects])

AC_CONFIG_MACRO_DIR([m4])
LT_INIT()

AC_PROG_CXX
AM_PROG_CC_C_O

AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([src/config.h])
AC_DEFINE_UNQUOTED([PREFIX_PATH], ["$prefix"], [Prefix path])

AC_OUTPUT
