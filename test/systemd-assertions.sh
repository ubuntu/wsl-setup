#!/bin/bash
# This is a set of basic assertions to verify systemd specific conditions in a newly set up WSL instance.
# This should run inside a WSL instance or machine prepared for testing purposes.
# It requires installing the wsl-setup Debian package to assert on its results.

if [[ $(LANG=C systemctl is-system-running) != "running" ]]; then
	systemctl --failed
	exit 1
fi

if [[ $(LANG=C systemctl is-active multipathd.service) != "inactive" ]]; then
	echo "::error:: Unit multipathd.service should have been disabled by the multipathd.service.d/container.conf override"
	systemctl status multipathd.service
	exit 2
fi

# It's been a while since WSL kernel implemented a patch specifically for time sync with Hyper-V.
# With that NTS clients inside WSL instances can cause more trouble if they don't sync with the same
# source as Windows does. So we're no longer overriding this systemd unit, let be the defaults.
# Let's not worry about chrony just yet.
nts_unit="systemd-timesyncd.service"
if systemctl is-enabled "${nts_unit}"; then
	if [[ $(LANG=C systemctl is-active "${nts_unit}") != "inactive" ]]; then
		echo "::error:: Unit ${nts_unit} should be disabled by default."
		systemctl status ${nts_unit}
		exit 3
	fi
fi

if [[ ! -r "/etc/cloud/cloud-init.disabled" ]]; then
	echo "::error:: Missing cloud-init.disabled marker file"
	exit 4
fi
