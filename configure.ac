AC_PREREQ([2.4])
AC_INIT([tipcutils], [2.0.3], [erik.hugne@ericsson.com])

AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([linux/tipc.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

CFLAGS="$CFLAGS -Wall -fno-strict-aliasing"

AC_CONFIG_FILES([
	Makefile
	tipc-config/Makefile
	tipc-pipe/Makefile
	ptts/Makefile
	demos/Makefile
	demos/benchmark/Makefile
	demos/connection_demo/Makefile
	demos/hello_world/Makefile
	demos/inventory_sim/Makefile
	demos/multicast_demo/Makefile
	demos/stream_demo/Makefile
	demos/topology_subscr_demo/Makefile
	])

AC_OUTPUT
