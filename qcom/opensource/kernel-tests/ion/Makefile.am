iondir			= $(prefix)/ion
ion_PROGRAMS		= msm_iontest
msm_iontest_SOURCES	= msm_iontest.c cp_ion_tests.c ion_test_utils.c kernel_ion_tests.c user_ion_tests.c
dist_ion_SCRIPTS	= iontest.sh run.sh


KERNEL_FLAGS ?= ARCH=arm

module = msm_ion_test_module.ko
kmake  = $(MAKE) $(KERNEL_FLAGS) -C $(KERNEL_DIR) M=$(CURDIR)

$(module):
	$(kmake) modules

all-local: $(module)

install-exec-local: $(module)
	$(kmake) INSTALL_MOD_PATH=$(DESTDIR)$(prefix)/modules modules_install

dist_ion_DATA		= README.txt $(module)

# "make distclean" will always run clean-local in this directory,
# regardless of the KERNELMODULES conditional. Therefore, ensure
# KERNEL_DIR exists before running clean. Further, don't fail even
# if there is a problem.
clean-local:
	-test ! -d "$(KERNEL_DIR)" || $(kmake) clean


