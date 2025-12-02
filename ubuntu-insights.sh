#!/bin/bash
# Manages Ubuntu Insights consent during WSL setup.
# It prioritizes any existing local consent settings, then the Windows registry.
# If no consent is found, it prompts the user for consent.
set -euo pipefail

sources=("" "linux" "wsl_setup" "ubuntu_release_upgrader")

registry_key_path="HKCU:\Software\Canonical\Ubuntu"
registry_value_name="UbuntuInsightsConsent"

# List of regular users
readarray -t users < <(getent passwd | grep -Ev '/nologin|/false|/sync' | awk -F: '$3 >= 1000 { print $1 }')

function ask_question() {
	# Only ask if we are in an interactive terminal
	if [ ! -t 0 ]; then
		return
	fi

	local choice_val=""
	local view_choice=""
	while true; do
		echo "Would you like to opt-in to platform metrics collection (Y/n)? To see an example of the data collected, enter 'e'."
		read -rep "[Y/n/e]: " -i "y" view_choice

		if [[ $view_choice =~ ^[Ee]$ ]]; then
			ubuntu-insights collect -df 2>/dev/null
			continue
		elif [[ $view_choice =~ ^[Yy]$ ]]; then
			choice_val=1
			break
		elif [[ $view_choice =~ ^[Nn]$ ]]; then
			choice_val=0
			break
		else
			echo "Invalid input (Y/n/e)."
		fi
	done

	apply_consent "$choice_val"
	set_consent_registry "$choice_val" >/dev/null || true
}

function apply_consent() {
	local consent="$1"

	for user in "${users[@]}"; do
		for source in "${sources[@]}"; do
			if [ -n "$source" ]; then
				su "$user" -c 'ubuntu-insights consent "$0" -s="$1" > /dev/null' -- "$source" "$([[ $consent -eq 1 ]] && echo true || echo false)"
			else
				su "$user" -c 'ubuntu-insights consent -s="$0" > /dev/null' -- "$([[ $consent -eq 1 ]] && echo true || echo false)"
			fi
		done
	done
}

function read_consent_registry() {
	local consent_value=""
	consent_value=$(powershell.exe -NoProfile -Command "& {
        param(\$Path, \$Name)
        [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
        try {  
            return (Get-ItemProperty -Path \$Path -Name \$Name -ErrorAction Stop).\$Name
        }
        catch {
            return \"\"
        }
    }" -Path "${registry_key_path}" -Name "${registry_value_name}" 2>/dev/null) || true
	# strip control chars like \r and \n
	echo "${consent_value//[[:cntrl:]]/}"
}

function set_consent_registry() {
	local consent="$1"
	powershell.exe -NoProfile -Command "& {
        param(\$Path, \$Name, \$Consent)
        if (-not (Test-Path -Path \$Path)) {
            New-Item -Path \$Path -Force | Out-Null
        }
        New-ItemProperty -Path \$Path -Name \$Name -Value \$Consent -PropertyType DWord -Force
    }" -Path "${registry_key_path}" -Name "${registry_value_name}" -Consent "${consent}" 2>/dev/null || true
}

function check_local_consent() {
	# If any of the users has default consent set, we consider that consent is set locally
	for user in "${users[@]}"; do
		# Check default, if exit code is 0, consent is set
		if su "$user" -c 'ubuntu-insights consent' >/dev/null 2>&1; then
			return 0
		fi
	done
	return 1
}

function collect() {
	# Collect insights and upload in background for all users
	for user in "${users[@]}"; do
		if su "$user" -c 'echo "{}" | ubuntu-insights collect wsl_setup /dev/stdin -f > /dev/null 2>&1'; then
			su "$user" -c 'nohup ubuntu-insights upload wsl_setup -rf > /dev/null 2>&1 &'
		fi
	done
}

# Check if ubuntu-insights is installed
if ! command -v ubuntu-insights >/dev/null 2>&1; then
	return 0 2>/dev/null || exit 0
fi

# Skip if no users found
if [ "${#users[@]}" -eq 0 ]; then
	return 0 2>/dev/null || exit 0
fi

# Check if consent is already set locally
if check_local_consent; then
	collect
	return 0 2>/dev/null || exit 0
fi

# Check if we have a stored consent value in the Windows registry.
consent_value=$(read_consent_registry)
if [[ "$consent_value" =~ ^[01]$ ]]; then
	apply_consent "$consent_value"
	collect
	return 0 2>/dev/null || exit 0
fi

# Failed to read consent from the Windows registry, ask the user.
echo "Help improve Ubuntu!

You can share anonymous data with the Ubuntu development team so we can improve your experience.
If you agree, we will collect and report anonymous hardware and system information.
This information can't be used to identify a single machine.
For legal details, please visit: https://ubuntu.com/legal/systems-information-notice

We will save your answer to Windows and will only ask you once.
"
ask_question

collect
