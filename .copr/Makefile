outdir ?= .

.PHONY: srpm

srpm:
	if ! command -v rpmbuild >/dev/null 2>&1 || ! command -v git >/dev/null 2>&1; then \
		dnf install -y rpm-build git-core; \
	fi
	set -eu; \
	VERSION=$$(sed -n 's/^project.*VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt); \
	GIT_SLUG=$$(git rev-parse --short HEAD); \
	TIMESTAMP=$$(date -u +%Y%m%d%H%M%S); \
	RELEASE="1.$${TIMESTAMP}git$${GIT_SLUG}"; \
	RPM_ROOT=$$(mktemp -d); \
	TARBALL="ec-controller-$${VERSION}.tar.gz"; \
	mkdir -p "$${RPM_ROOT}/SOURCES" "$${RPM_ROOT}/SPECS" "$${RPM_ROOT}/SRPMS"; \
	git archive --format=tar.gz --prefix="ec-controller-$${VERSION}/" HEAD -o "$${RPM_ROOT}/SOURCES/$${TARBALL}"; \
	sed -e "s/^Version:.*/Version:        $${VERSION}/" \
	    -e "s/^Release:.*/Release:        $${RELEASE}%{?dist}/" \
	    packaging/ec-controller.spec > "$${RPM_ROOT}/SPECS/ec-controller.spec"; \
	TZ=UTC LC_ALL=C git log -n 10 --date-order --date=format-local:"%a %b %d %Y" \
		--pretty=tformat:"* %cd %an <%ae> - $${VERSION}-$${RELEASE}%n- %s (%h)" \
		| sed 's/%/%%/g' >> "$${RPM_ROOT}/SPECS/ec-controller.spec"; \
	rpmbuild -bs \
		--define "_topdir $${RPM_ROOT}" \
		--define "_srcrpmdir $(outdir)" \
		"$${RPM_ROOT}/SPECS/ec-controller.spec"; \
	rm -rf "$${RPM_ROOT}"
