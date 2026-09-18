# SPDX-License-Identifier: GPL-2.0

KBUILD_OPTIONS+= KERNEL_ROOT=$(ROOT_DIR)/$(KERNEL_DIR)

all: modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(@)

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(@)

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(@) $(KBUILD_OPTIONS)