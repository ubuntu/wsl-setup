#!/bin/bash
# This is a set of basic assertions partially validating Ubuntu Insights consent setup in WSL.
# This should run inside a WSL instance or machine prepared for testing purposes.
# It requires installing the wsl-setup Debian package to assert on its results.
# The expected consent state can be passed as the first argument: true or false.

EXPECTED_CONSENT=${1}

if [[ "$EXPECTED_CONSENT" != "true" && "$EXPECTED_CONSENT" != "false" ]]; then
	echo "::error:: Expected first argument to be 'true' or 'false', got '$EXPECTED_CONSENT'"
	exit 1
fi

if ! ubuntu-insights consent | grep -q "Default: $EXPECTED_CONSENT"; then
	echo "::error:: Ubuntu-Insights Consent state assertion: Expected 'Default: $EXPECTED_CONSENT'"
	exit 1
fi

# Verify that a report was created for the wsl_setup source.
# Use max int32 for period (2147483647) to ensure we cover the timestamp of the existing report.
OUTPUT=$(ubuntu-insights collect --period 2147483647 --dry-run wsl_setup <(echo "{}") 2>&1 >/dev/null)
EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
	echo "::error:: Expected non-zero exit code from ubuntu-insights collect, got 0."
	exit 1
fi

if ! echo "$OUTPUT" | grep -q "report already exists for this period"; then
	echo "::error:: Expected 'report already exists for this period' error from ubuntu-insights collect."
	echo "Output was: $OUTPUT"
	exit 1
fi
