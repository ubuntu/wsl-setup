#!/bin/bash
# Unit tests for powershell_env(). powershell.exe is stubbed with a plain shell
# script, so this runs anywhere bash does -- no WSL or Windows required.
#
# Usage: powershell-env-test.sh [path-to-wsl-setup]
set -u

script="${1:-$(dirname "$(realpath "$0")")/../wsl-setup}"

# Loading the script only defines its functions; the main guard keeps provisioning
# from running. The path is dynamic, so ShellCheck cannot follow it.
# shellcheck disable=SC1090,SC1091
source "$script"
# wsl-setup enables -euo pipefail on source; this test checks exit codes itself.
set +e +u
set +o pipefail

workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT

failures=0

# stub_powershell <name>: reads a shell script body on stdin, installs it as
# powershell.exe, and prints the directory to prepend to PATH.
stub_powershell() {
	local dir="${workdir}/$1"
	mkdir -p "$dir"
	{
		printf '#!/bin/bash\n'
		cat
	} >"${dir}/powershell.exe"
	chmod +x "${dir}/powershell.exe"
	printf '%s' "$dir"
}

# assert_powershell_env <description> <stub-dir> <expected-value>
assert_powershell_env() {
	local description="$1" stubdir="$2" expected="$3" out err errfile
	errfile="${workdir}/stderr"
	out=$(
		export PATH="${stubdir}:${PATH}"
		powershell_env "SomeVar" 2>"${errfile}"
	)
	err=$(cat "${errfile}")

	if [ -n "${err}" ]; then
		echo "FAIL: ${description}: unexpected output on stderr: ${err}"
		failures=$((failures + 1))
	fi
	if [ "${out}" != "${expected}" ]; then
		echo "FAIL: ${description}: expected '${expected}', got '${out}'"
		failures=$((failures + 1))
	fi
}

# A powershell.exe that fails to start and prints a diagnostic: nothing may reach stderr.
failing=$(stub_powershell failing <<'PSEOF'
echo "WSL(interop) ERROR: execv(powershell.exe) failed: Exec format error" >&2
exit 1
PSEOF
)
assert_powershell_env "failing powershell.exe is silent" "${failing}" ""

# A working powershell.exe: the value must pass through unchanged.
working=$(stub_powershell working <<'PSEOF'
printf '%s' 'C:\Users\Test'
PSEOF
)
assert_powershell_env "value is returned unchanged" "${working}" 'C:\Users\Test'

if [ "${failures}" -ne 0 ]; then
	echo "${failures} assertion(s) failed"
	exit 1
fi
echo "All powershell_env tests passed"

