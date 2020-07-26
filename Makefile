MODNAME = qc71_laptop
MODVER = 0.1


obj-m += $(MODNAME).o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean


dkmsinstall:
	mkdir -p /usr/src/$(MODNAME)-$(MODVER)
	cp Makefile dkms.conf $(MODNAME).c /usr/src/$(MODNAME)-$(MODVER)/.
	dkms add $(MODNAME)/$(MODVER) && \
	dkms build $(MODNAME)/$(MODVER) && \
	dkms install $(MODNAME)/$(MODVER)

dkmsuninstall:
	-rmmod $(MODNAME)
	dkms uninstall $(MODNAME)/$(MODVER)
	dkms remove $(MODNAME)/$(MODVER) --all
	rm -rf /usr/src/$(MODNAME)-$(MODVER)
