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
