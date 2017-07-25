memory_profdir = $(prefix)/memory_prof
memory_prof_PROGRAMS = memory_prof memfeast
memory_prof_SOURCES	= memory_prof.c alloc_profiles.c memory_prof_util.c
memory_prof_CFLAGS = -lm
memfeast_SOURCES = memfeast.c memory_prof_util.c
dist_memory_prof_SCRIPTS  = memory_prof.sh run.sh
dist_memory_prof_DATA = README.txt $(wildcard alloc_profiles/*)

KERNEL_FLAGS ?= ARCH=arm

all_modules = the_memory_prof_module.ko

kmake  = $(MAKE) $(KERNEL_FLAGS) -C $(KERNEL_DIR) M=$(CURDIR)

the_memory_prof_module.ko: memory_prof_module.c timing_debug.c
	$(kmake) modules

all-local: $(all_modules)

install-exec-local: $(all_modules)
	$(kmake) INSTALL_MOD_PATH=$(DESTDIR)$(prefix)/modules modules_install

# "make distclean" will always run clean-local in this directory,
# regardless of the KERNELMODULES conditional. Therefore, ensure
# KERNEL_DIR exists before running clean. Further, don't fail even
# if there is a problem.
clean-local:
	-test ! -d "$(KERNEL_DIR)" || $(kmake) clean
