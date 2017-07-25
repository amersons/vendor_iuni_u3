cs_profiledir           = $(prefix)

cs_profile_PROGRAMS     = cs_profile_mod

PWD := $(shell pwd)

all-local:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

install-exec-local:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(DESTDIR)$(prefix) modules_install
	depmod $(shell cat $(KERNELDIR)/include/config/kernel.release 2> /dev/null) -b $(DESTDIR)$(prefix)
