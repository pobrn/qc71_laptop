# SPDX-License-Identifier: GPL-2.0
MODNAME = qc71_laptop
MODVER = 0.1

obj-m += $(MODNAME).o

# alphabetically sorted
$(MODNAME)-y += ec.o \
		features.o \
		main.o \
		misc.o \
		pdev.o \
		events.o \

$(MODNAME)-$(CONFIG_DEBUG_FS)     += debugfs.o
$(MODNAME)-$(CONFIG_ACPI_BATTERY) += battery.o
$(MODNAME)-$(CONFIG_LEDS_CLASS)   += led_lightbar.o
$(MODNAME)-$(CONFIG_HWMON)        += hwmon.o hwmon_fan.o hwmon_pwm.o fan.o

KDIR = /lib/modules/$(shell uname -r)/build
MDIR = /usr/src/$(MODNAME)-$(MODVER)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

dkmsinstall:
	mkdir -p $(MDIR)
	cp Makefile dkms.conf $(wildcard *.c) $(wildcard *.h) $(MDIR)/.
	dkms add $(MODNAME)/$(MODVER)
	dkms build $(MODNAME)/$(MODVER)
	dkms install $(MODNAME)/$(MODVER)

dkmsuninstall:
	-rmmod $(MODNAME)
	-dkms uninstall $(MODNAME)/$(MODVER)
	-dkms remove $(MODNAME)/$(MODVER) --all
	rm -rf $(MDIR)
