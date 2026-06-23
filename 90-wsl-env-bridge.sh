#!/bin/bash
# This script is used to bridge environment variables seeing by systemd into the environment of a user shell session
# by means of re-exporting the output of (systemctl show-environment) into the shell session.

if [ -n "$BASH_VERSION" ] || [ -n "$ZSH_VERSION" ]; then
    # Export the environment variables from systemd into the current shell session
    while IFS='=' read -r line; do
      # For now at least only exporting locale-related variables.
      if [[ "$line" =~ ^(LANG|LC_[A-Z]+)= ]]; then
	# shellcheck disable=SC2163 # Intentionally exporting the contents, not the variable itself.
        export "$line"
      fi
    done < <(systemctl show-environment)
else
    echo "This script is intended to be sourced in a bash or zsh shell."
fi 
