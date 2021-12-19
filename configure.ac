AC_PREREQ([2.61])
AC_INIT([neo], [0.6])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign])
AC_LANG(C++)
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_CHECK_LIB(ncursesw, mvadd_wch)
AC_CHECK_HEADERS(getopt.h locale.h ncurses.h)

dnl Some systems have both ncurses.h and ncursesw/ncurses.h.
dnl On many systems, the headers are identical (e.g. Ubuntu),
dnl but for some systems they differ. So we should always try
dnl to use ncursesw/ncurses.h if it exists.
AC_CHECK_HEADER(ncursesw/ncurses.h, AC_DEFINE(HAVE_NCURSESW_H))

dnl Some systems build ncurses with a separate "termlib" library.
dnl This will usually be libtinfo or less frequently libtinfow.
dnl These libraries provide functions that do not depend on whether
dnl or not ncurses is using widechars (e.g. cbreak). This line adds
dnl -ltinfow or -ltinfo to LIBS, if needed. If libncursesw
dnl already provides cbreak, then the configure script should
dnl print a message saying "none required", but it should not fail.
AC_SEARCH_LIBS(cbreak, [tinfow tinfo])

AC_CONFIG_FILES([Makefile doc/Makefile src/Makefile])
AC_OUTPUT
