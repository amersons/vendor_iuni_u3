iommudir		= $(prefix)/msm_iommu
iommu_PROGRAMS		= msm_iommutest
msm_iommutest_SOURCES	= msm_iommutest.c
dist_iommu_SCRIPTS	= iommutest.sh run.sh
module			= msm_iommu_test_module.ko
dist_iommu_DATA		= README.txt apq8084_lpae_bfb.txt apq8084_bfb.txt msm8974_bfb.txt msm8226_bfb.txt mpq8092_bfb.txt $(module)

KERNEL_FLAGS ?= ARCH=arm

kmake  = $(MAKE) $(KERNEL_FLAGS) -C $(KERNEL_DIR) M=$(CURDIR)

$(module):
	$(kmake) modules

all-local: $(module)

install-exec-local: $(module)
	$(kmake) INSTALL_MOD_PATH=$(DESTDIR)$(prefix)/modules modules_install

# "make distclean" will always run clean-local in this directory,
# regardless of the KERNELMODULES conditional. Therefore, ensure
# KERNEL_DIR exists before running clean. Further, don't fail even
# if there is a problem.
clean-local:
	-test ! -d "$(KERNEL_DIR)" || $(kmake) clean


