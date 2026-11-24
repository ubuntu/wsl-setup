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

# --- Real PowerShell engine -------------------------------------------------
# The bash stub above never parses the PowerShell payload, so on its own it
# cannot catch a malformed -Command (e.g. unbalanced braces) or the Constrained
# Language Mode path. When a real engine is present -- pwsh ships on the Linux
# CI runners -- drive the real payload through it. Skipped when pwsh is absent.
if command -v pwsh >/dev/null 2>&1; then
	# engine_shim <name>: a powershell.exe that hands the -Command payload to
	# pwsh, preceded by $POWERSHELL_ENV_PREAMBLE when that is set (used to force
	# Constrained Language Mode).
	engine_shim() {
		local dir="${workdir}/$1"
		mkdir -p "$dir"
		cat >"${dir}/powershell.exe" <<'SHIM'
#!/bin/bash
preamble="${POWERSHELL_ENV_PREAMBLE:-}"
prev=""
for arg in "$@"; do
	if [ "${prev}" = "-Command" ]; then
		exec pwsh -NoProfile -NonInteractive -Command "${preamble}${arg}"
	fi
	prev="${arg}"
done
echo "powershell.exe shim: no -Command argument" >&2
exit 1
SHIM
		chmod +x "${dir}/powershell.exe"
		printf '%s' "$dir"
	}

	engine=$(engine_shim engine)

	# The real engine must parse the payload and hand the value back unchanged.
	export SomeVar='C:\Users\Test Value'
	unset POWERSHELL_ENV_PREAMBLE
	assert_powershell_env "real engine returns the value" "${engine}" 'C:\Users\Test Value'

	# Under Constrained Language Mode the console-encoding assignment is
	# rejected; the payload's try/catch must fail closed (no value, no stderr).
	# shellcheck disable=SC2016 # Intentional: the value is a PowerShell fragment, not shell
	export POWERSHELL_ENV_PREAMBLE='$ExecutionContext.SessionState.LanguageMode = "ConstrainedLanguage"; '
	assert_powershell_env "Constrained Language Mode fails closed" "${engine}" ""
	unset POWERSHELL_ENV_PREAMBLE
else
	echo "SKIP: pwsh not installed; real-engine checks not run"
fi

if [ "${failures}" -ne 0 ]; then
	echo "${failures} assertion(s) failed"
	exit 1
fi
echo "All powershell_env tests passed"

