AC_PREREQ(2.64)

AC_INIT([kernel-tests],[1.0.0])
AM_INIT_AUTOMAKE([-Wall -Werror gnu foreign])

AM_MAINTAINER_MODE

AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_ARG_WITH(sanitized-headers,
	AS_HELP_STRING([--with-sanitized-headers=DIR],
		[Specify the location of the sanitized Linux headers]),
	[CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_ENABLE(big-tests,
	AS_HELP_STRING([--enable-big-tests],
		[Include tests that use a lot of disk [default=yes]]),
	[case "${enableval}" in
	  yes) BIGTESTS=true ;;
	  no)  BIGTESTS=false ;;
	  *)   AC_MSG_ERROR([bad value ${enableval} for --enable-big-tests]) ;;
	esac],
	[BIGTESTS=true])
AM_CONDITIONAL(BIGTESTS, [test x$BIGTESTS = xtrue])

AC_ARG_WITH([kernel],
	AS_HELP_STRING([--with-kernel=DIR],
		[location of a built kernel; used for building kernel modules]),
	[case "${with_kernel}" in
	  yes|'') AC_MSG_ERROR([--with-kernel must specify a path]) ;;
	  no)     with_kernel= ;;
	  *)
		if [test ! -f ${with_kernel}/Makefile]; then
			AC_MSG_ERROR([bad value '${with_kernel}' for --with-kernel; cannot find '${with_kernel}/Makefile'])
		fi ;;
	esac])
if [test -z "${with_kernel}"]; then
	AC_MSG_WARN([no kernel specified; kernel modules will not be included])
else
	AC_SUBST([KERNEL_DIR], [${with_kernel}])
fi
AM_CONDITIONAL(KERNELMODULES, [test -n "${with_kernel}"])

#
# Build all kernel tests by default
#
# To add new tests add ENABLE_KERNEL_TEST and update AC_CONFIG_FILES
#
# Tests which will be too large to fit in an initrd/initramfs should not
# be installed if the conditional BIGTESTS is false.
#

# ion
ENABLE_KERNEL_TEST([ion],[include the ion test (default=yes)])

# memory_prof
ENABLE_KERNEL_TEST([memory_prof],[include the memory profiling test (default=yes)])

# msm-bus
ENABLE_KERNEL_TEST([msm-bus],[include the MSM bus test (default=yes)])

# sps
ENABLE_KERNEL_TEST([sps],[include the sps test (default=yes)])

# v4l2apps
ENABLE_KERNEL_TEST([v4l2apps],[include the v4l2apps test (default=no)],[no])

# watchdog
ENABLE_KERNEL_TEST([watchdog],[include the watchdog test (default=yes)])

# ocmem
ENABLE_KERNEL_TEST([ocmem],[include the ocmem test (default=yes)])

# iommu
ENABLE_KERNEL_TEST([iommu],[include the iommu test (default=yes)])


AC_SUBST([CFLAGS])
AC_SUBST([CC])

AC_CONFIG_FILES([
	Makefile
	ion/Makefile
	memory_prof/Makefile
	msm_bus/Makefile
	ocmem/Makefile
	msm_iommu/Makefile
	sps/Makefile
	v4l2apps/Makefile
	watchdog/Makefile
	])
AC_OUTPUT
