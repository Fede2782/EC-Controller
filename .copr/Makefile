outdir ?= .

VERSION := $(shell sed -n 's/^project(msicontroller VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
GIT_SLUG := $(shell git rev-parse --short HEAD)
TIMESTAMP := $(shell date -u +%Y%m%d%H%M%S)
RELEASE := 1.$(TIMESTAMP)git$(GIT_SLUG)
RPM_ROOT := $(CURDIR)/rpmbuild
TARBALL := ec-controller-$(VERSION).tar.gz

.PHONY: srpm

srpm:
	if ! command -v rpmbuild >/dev/null 2>&1 || ! command -v git >/dev/null 2>&1; then \
		dnf install -y rpm-build git; \
	fi
	mkdir -p "$(RPM_ROOT)/SOURCES" "$(RPM_ROOT)/SPECS" "$(RPM_ROOT)/SRPMS"
	git archive --format=tar.gz --prefix="ec-controller-$(VERSION)/" HEAD -o "$(RPM_ROOT)/SOURCES/$(TARBALL)"
	cp packaging/ec-controller.spec "$(RPM_ROOT)/SPECS/ec-controller.spec"
	TZ=UTC LC_ALL=C git log -n 10 --date-order --date=format-local:"%a %b %d %Y" \
		--pretty=format:"* %cd %an <%ae> - $(VERSION)-$(RELEASE)%n- %s (%h)%n" \
		| sed 's/%/%%/g' >> "$(RPM_ROOT)/SPECS/ec-controller.spec"
	rpmbuild -bs \
		--define "_topdir $(RPM_ROOT)" \
		--define "_srcrpmdir $(outdir)" \
		--define "pkg_version $(VERSION)" \
		--define "pkg_release $(RELEASE)" \
		"$(RPM_ROOT)/SPECS/ec-controller.spec"
	rm -rf "$(RPM_ROOT)"
