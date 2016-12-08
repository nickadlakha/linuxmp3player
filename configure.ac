#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.64])
AC_INIT([player], [1.0], [nicklesh.adlakha@gmail.com])
AM_INIT_AUTOMAKE(player, 1.0)
AC_CONFIG_SRCDIR([player.c])
#AC_CONFIG_HEADERS([config.h])

if test 1; then
CFLAGS="-D_FILE_OFFSET_BITS=64 -O2"
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_STDC
# Checks for libraries.
AC_CHECK_LIB([mpg123], [mpg123_open_64], [], [AC_MSG_ERROR(didn't find libmpg123.so)], [])

AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR(didn't find thread library)], [])

AC_CHECK_LIB([asound], [snd_pcm_open], [], [AC_MSG_ERROR(didn't find libasound.so)], [])

AC_CHECK_LIB([magic], [magic_open], [], [AC_MSG_ERROR(didn't find libmagic.so)], [])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h netdb.h stdlib.h string.h sys/ioctl.h sys/socket.h unistd.h mpg123.h alsa/asoundlib.h magic.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gethostbyname select socket strstr])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
