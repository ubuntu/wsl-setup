#!/bin/bash
# This should run inside a WSL instance or machine prepared for testing purposes.
# It requires installing the wsl-setup Debian package to assert on their results.
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

if [[ $(whoami) != "u" ]]; then
  echo "::error:: Default user doesn't match cloud-init's user-data."
  exit 4
fi

if [[ $(LANG=C systemctl is-system-running) != "running" ]]; then
   systemctl --failed
   exit 5
fi

if [[ $(LANG=C systemctl is-active multipathd.service) != "inactive" ]]; then
  echo "::error:: Unit multipathd.service should have been disabled by the multipathd.service.d/container.conf override"
  systemctl status multipathd.service
  exit 6
fi

# Let's not worry about chrony just yet.
nts_unit="systemd-timesyncd.service"
if systemctl is-enabled "${nts_unit}"; then
  if [[ $(LANG=C systemctl is-active "${nts_unit}" ) != "active" ]]; then
    echo "::error:: Unit ${nts_unit} should be enabled in WSL via ${nts_unit}.d/wsl.conf override"
    systemctl status ${nts_unit}
    exit 7
  fi
fi

if [[ ! -r "/etc/cloud/cloud-init.disabled" ]]; then
  echo "::error:: Missing cloud-init.disabled marker file"
  exit 8
fi

if ! hello; then
  echo "::error:: Failed to execute the hello program that should have been installed by cloud-init"
  exit 9
fi
