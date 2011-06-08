printchanges:
	baseCommit=$$(git log --pretty=format:'%H %s' | \
		awk '/UBUNTU: '".*Ubuntu-$(release)-$(prev_revision)"'$$/ { print $$1; exit }'); \
		git log "$$baseCommit"..HEAD | \
		perl -w -f debian/scripts/misc/git-ubuntu-log $(ubuntu_log_opts)

insertchanges:
	@perl -w -f debian/scripts/misc/insert-changes.pl

startnewrelease:
	dh_testdir
	@nextminor=$(shell expr `echo $(revision) | awk -F. '{print $$2}'` + 1); \
	now="$(shell date -R)"; \
	echo "Creating new changelog set for $(release)-$(abinum).$$nextminor..."; \
	echo -e "linux-backports-modules-$(release) ($(release)-$(abinum).$$nextminor) UNRELEASED; urgency=low\n" > debian/changelog.new; \
	echo "  CHANGELOG: Do not edit directly. Autogenerated at release." >> \
		debian/changelog.new; \
	echo "  CHANGELOG: Use the printchanges target to see the curent changes." \
		>> debian/changelog.new; \
	echo "  CHANGELOG: Use the insertchanges target to create the final log." \
		>> debian/changelog.new; \
        echo -e "\n -- $$DEBFULLNAME <$$DEBEMAIL>  $$now\n" >> \
                debian/changelog.new ; \
	cat debian/changelog >> debian/changelog.new; \
	mv debian/changelog.new debian/changelog

#
# If $(ppa_file) exists, then only the standard flavours are built for PPA, e.g.,
# 386, 386-generic, and amd64-generic.
#
prepare-ppa:
	@echo Execute debian/scripts/misc/prepare-ppa-source to prepare an upload
	@echo for a PPA build. You must do this outside of debian/rules since it cannot
	@echo nest.

print-ppa-file-name:
	@echo $(ppa_file)

