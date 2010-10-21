# We don't want make removing intermediary stamps
.SECONDARY :

build-arch: $(addprefix build-modules-,$(flavours))

# Prepare the out-of-tree build directory

printenv:
	@echo "confdir           =  $(confdir)"
	@echo "stampdir          = $(stampdir)"
	@echo "release           = $(release)"
	@echo "revisions         = $(revisions)"
	@echo "revision          = $(revision)"
	@echo "prev_revisions    = $(prev_revisions)"
	@echo "prev_revision     = $(prev_revision)"
	@echo "abinum            = $(abinum)"
	@echo "flavours          = $(flavours)"

COMPAT_KDIR=/lib/modules/$(release)-$(abinum)-$(target_flavour)
make_compat = make $(conc_level)
make_compat += KLIB=$(COMPAT_KDIR) KLIB_BUILD=$(COMPAT_KDIR)/build
make_compat += MADWIFI=
make_compat += OLD_IWL=

ifneq ($(LOCAL_ENV_CC),)
make_compat += CC=$(LOCAL_ENV_CC) DISTCC_HOSTS=$(LOCAL_ENV_DISTCC_HOSTS)
endif

prepare-%: $(stampdir)/stamp-prepare-%
	@# Empty for make to be happy
$(stampdir)/stamp-prepare-%: target_flavour = $*
$(stampdir)/stamp-prepare-%: cw = $(builddir)/build-$*/compat-wireless-2.6
$(stampdir)/stamp-prepare-%: $(confdir)/$(arch)
	@echo "Preparing $*..."
	install -d $(builddir)/build-$*
	cd updates; tar cf - * | tar -C $(builddir)/build-$* -xf -
ifeq ($(do_compat_wireless),true)
	# Gross hackery to make the compat firmware class unique to this ABI
	sed -i 's/compat_firmware/compat_firmware_'$(abinum)'/g' $(cw)/compat/compat_firmware_class.c $(cw)/compat/scripts/compat_firmware_install \
		$(cw)/udev/ubuntu/50-compat_firmware.rules
	mv $(cw)/udev/ubuntu/50-compat_firmware.rules $(cw)/udev/ubuntu/50-compat_firmware_$(abinum).rules
	mv $(cw)/udev/ubuntu/compat_firmware.sh $(cw)/udev/ubuntu/compat_firmware_$(abinum).sh
endif
ifeq ($(do_alsa),true)
	cd $(builddir)/build-$*/alsa-driver && ./configure --with-kernel=$(COMPAT_KDIR)/build
endif
	cat $^ > $(builddir)/build-$*/.config
	# XXX: generate real config
	touch $(builddir)/build-$*/ubuntu-config.h
	touch $(builddir)/build-$*/ubuntu-build
	touch $@

# Do the actual build, including image and modules
build-modules-%: $(stampdir)/stamp-build-%
	@# Empty for make to be happy
$(stampdir)/stamp-build-%: target_flavour = $*
$(stampdir)/stamp-build-%: build_arch_t = $(call custom_override,build_arch,$*)
$(stampdir)/stamp-build-%: $(stampdir)/stamp-prepare-%
	@echo "Building $*..."
ifeq ($(do_compat_wireless),true)
	cd $(builddir)/build-$*/compat-wireless-2.6 && $(make_compat)
endif
ifeq ($(do_alsa),true)
	cd $(builddir)/build-$*/alsa-driver && make $(conc_level)
endif
	$(kmake) $(conc_level) modules
	@touch $@

# Install the finished build
install-%: impkgdir = $(CURDIR)/debian/linux-backports-modules-input-$(release)-$(abinum)-$*
install-%: immoddir = $(impkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: cwpkgdir = $(CURDIR)/debian/linux-backports-modules-wireless-$(release)-$(abinum)-$*
install-%: cwblddir = $(builddir)/build-$*/compat-wireless-2.6
install-%: cwmoddir = $(cwpkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: cwsrcdir = $(CURDIR)/updates/compat-wireless-2.6
install-%: cspkgdir = $(CURDIR)/debian/linux-backports-modules-alsa-$(release)-$(abinum)-$*
install-%: csmoddir = $(cspkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: firmdir = $(cwpkgdir)/lib/firmware/updates/$(release)-$(abinum)-$*
install-%: netpkgdir = $(CURDIR)/debian/linux-backports-modules-net-$(release)-$(abinum)-$*
install-%: netmoddir = $(netpkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: lbmbasehdrpkg = linux-headers-lbm-$(release)$(debnum)
install-%: lbmhdrpkg = $(lbmbasehdrpkg)-$*
install-%: hdrdir = $(CURDIR)/debian/$(lbmhdrpkg)/usr/src/$(lbmhdrpkg)
install-%: target_flavour = $*
install-%: $(stampdir)/stamp-build-%
	dh_testdir
	dh_testroot
	dh_clean -k -plinux-backports-modules-wireless-$(release)-$(abinum)-$*

	install -d $(firmdir)
	#
	# This firmware file name has to be consistent with IWL4965_UCODE_API in iwl4965-base.c
	#
	cp firmware/iwlwifi/*4965*/*.ucode $(firmdir)/lbm-iwlwifi-4965-2.ucode
	cp firmware/iwlwifi/*3945*/*.ucode $(firmdir)/lbm-iwlwifi-3945-2.ucode
	cp firmware/iwlwifi/*5000*/*.ucode $(firmdir)/lbm-iwlwifi-5000-1.ucode
	cp firmware/iwlwifi/*5150*/*.ucode $(firmdir)/lbm-iwlwifi-5150-1.ucode
	cp firmware/iwlwifi/*6000*/*.ucode $(firmdir)/lbm-iwlwifi-6000-4.ucode
	cp firmware/iwlwifi/*6050*/*.ucode $(firmdir)/lbm-iwlwifi-6050-4.ucode

ifeq ($(do_compat_wireless),true)
	#
	# Build the compat wireless packages.
	#
	install -d $(cwmoddir)/updates/cw
	find $(builddir)/build-$*/compat-wireless-2.6 -type f -name '*.ko' | \
	while read f ; do \
		cp -v $${f} $(cwmoddir)/updates/cw/`basename $${f}`; \
	done

	find $(cwpkgdir)/ -type f -name \*.ko -print | xargs -r strip --strip-debug

	install -d $(cwpkgdir)/DEBIAN
	for script in postinst postrm; do					\
	  sed -e 's/@@KVER@@/$(release)-$(abinum)-$*/g'				\
	       debian/control-scripts/$$script > $(cwpkgdir)/DEBIAN/$$script;	\
	  chmod 755 $(cwpkgdir)/DEBIAN/$$script;					\
	done

	#
	# the compat_firmware_class has its own rules.
	#
	install -d $(cwpkgdir)/lib/udev
	install --mode=0755 $(cwblddir)/udev/ubuntu/compat_firmware_$(abinum).sh $(cwpkgdir)/lib/udev
	install -d $(cwpkgdir)/lib/udev/rules.d
	install --mode=0644 $(cwblddir)/udev/ubuntu/50-compat_firmware_$(abinum).rules $(cwpkgdir)/lib/udev/rules.d
endif

ifeq ($(do_alsa),true)
	#
	# Build the ALSA snapshot packages.
	#
	install -d $(csmoddir)/updates/alsa
	find $(builddir)/build-$*/alsa-driver -type f -name '*.ko' | while read f ; do cp -v $${f} $(csmoddir)/updates/alsa/`basename $${f}`; done

	find $(cspkgdir)/ -type f -name \*.ko -print | xargs -r strip --strip-debug

	install -d $(cspkgdir)/DEBIAN
	for script in postinst postrm; do					\
	  sed -e 's/@@KVER@@/$(release)-$(abinum)-$*/g'				\
	       debian/control-scripts/$$script > $(cspkgdir)/DEBIAN/$$script;	\
	  chmod 755 $(cspkgdir)/DEBIAN/$$script;					\
	done
endif

ifeq ($(do_net),true)
	#
	# Build the network package.
	#
	install -d $(netmoddir)/updates/net
	find $(builddir)/build-$*/net -type f -name '*.ko' | \
	while read f ; do \
		cp -v $${f} $(netmoddir)/updates/net/`basename $${f}`; \
	done

	find $(netpkgdir)/ -type f -name \*.ko -print | xargs -r strip --strip-debug

	install -d $(netpkgdir)/DEBIAN
	for script in postinst postrm; do					\
	  sed -e 's/@@KVER@@/$(release)-$(abinum)-$*/g'				\
	       debian/control-scripts/$$script > $(netpkgdir)/DEBIAN/$$script;	\
	  chmod 755 $(netpkgdir)/DEBIAN/$$script;					\
	done
endif

ifeq ($(do_input),true)
	#
	# Build the input-drivers package.
     	#
	install -d $(immoddir)/updates/input
	find $(builddir)/build-$*/input-drivers -type f -name '*.ko' |         \
	while read f; do                                                       \
		install -v $$f $(immoddir)/updates/input/;                     \
		strip --strip-debug $(immoddir)/updates/input/$$(basename $$f);\
	done

	install -d $(impkgdir)/DEBIAN
	for script in postinst postrm; do				       \
	  sed -e 's/@@KVER@@/$(release)-$(abinum)-$*/g'			       \
	       debian/control-scripts/$$script > $(impkgdir)/DEBIAN/$$script;  \
	  chmod 755 $(impkgdir)/DEBIAN/$$script;			       \
	done
endif

	#
	# The flavour specific headers package
	#
	install -d $(hdrdir)/include
ifeq ($(do_compat_wireless),true)
	tar -C $(builddir)/build-$*/compat-wireless-2.6 -chf - include | tar -C $(hdrdir) -xf -
endif
ifeq ($(do_alsa),true)
	for i in asm linux media sound; do \
		tar -C $(builddir)/build-$*/alsa-driver/include -chf - $$i | tar -C $(hdrdir)/include -xf -; \
	done
endif
ifeq ($(do_net),true)
	tar -C $(builddir)/build-$*/net -chf - include | tar -C $(hdrdir) -xf -
endif
	dh_testdir
	dh_testroot
	dh_installchangelogs -p$(lbmhdrpkg)
	dh_installdocs -p$(lbmhdrpkg)
	dh_compress -p$(lbmhdrpkg)
	dh_fixperms -p$(lbmhdrpkg)
	dh_installdeb -p$(lbmhdrpkg)
	dh_gencontrol -p$(lbmhdrpkg)
	dh_md5sums -p$(lbmhdrpkg)
	dh_builddeb -p$(lbmhdrpkg)

package_list =
package_list += linux-backports-modules-wireless-$(release)-$(abinum)-$*
package_list += linux-backports-modules-alsa-$(release)-$(abinum)-$*
package_list += linux-backports-modules-net-$(release)-$(abinum)-$*
package_list += linux-backports-modules-input-$(release)-$(abinum)-$*

binary-modules-%: install-%
	dh_testdir
	dh_testroot

	for i in $(package_list) ; do \
	dh_installchangelogs -p$$i; \
	dh_installdocs -p$$i; \
	dh_compress -p$$i; \
	dh_fixperms -p$$i; \
	dh_installdeb -p$$i; \
	dh_gencontrol -p$$i; \
	dh_md5sums -p$$i; \
	dh_builddeb -p$$i -- -Zbzip2 -z9; \
	done

binary-debs: $(addprefix binary-modules-,$(flavours))
binary-arch: binary-debs binary-udebs
