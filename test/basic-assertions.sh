#!/bin/bash
# This is a set of basic assertions to verify a WSL instance is correctly set up.
# This should run inside a WSL instance or machine prepared for testing purposes.
# It requires installing the wsl-setup Debian package to assert on its results.
# The expected default user can be passed as the first argument.

EXPECTED_USER=${1:-u}

brandingdir="/usr/share/wsl"
if [[ ! -r "${brandingdir}/ubuntu.ico" ]]; then
	echo "::error:: Missing Ubuntu icon in the $brandingdir directory."
	ls "${brandingdir}"
	exit 1
fi

if [[ ! -r "${brandingdir}/terminal-profile.json" ]]; then
	echo "::error:: Missing terminal profile fragment in the $brandingdir directory."
	ls "${brandingdir}"
	exit 2
fi

if [[ $(id -u) == 0 ]]; then
	echo "::error:: Default user shouldn't be root"
	exit 3
fi

if [[ $(whoami) != "$EXPECTED_USER" ]]; then
	echo "::error:: Default user doesn't match expected user '$EXPECTED_USER'."
	exit 4
fi

# WSL only sets LANG from /etc/default/locale. We attempt to set the LC_* variables from the systemd
# environment. This test assumes that cloud-init set locale to en_US.UTF-8 but LC_NUMERIC to
# pt_BR.UTF-8 if systemd is ON (see .github/workflows/integration.yaml:15).
# Without systemd and cloud-init the default locale would be C.UTF-8 so the expectations still hold.
expected="C.UTF-8"
systemd_running="OFF"
if systemctl is-system-running --quiet; then
	expected="pt_BR.UTF-8"
	systemd_running="ON"
fi
if [[ "$LC_NUMERIC" != "$expected" ]]; then
	echo "::error:: With systemd $systemd_running, expected LC_NUMERIC to be $expected but got $LC_NUMERIC."
	echo "Expected locale:"
	cat /etc/default/locale
	echo "got locale:"
	locale
	echo "systemd environment:"
	systemctl show-environment
	exit 5
fi
