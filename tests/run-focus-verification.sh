#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TEST_TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/hypervibe-focus-tests.XXXXXX")"
trap 'rm -rf "$TEST_TMP_DIR"' EXIT
swiftc -module-cache-path "$TEST_TMP_DIR/module-cache" \
    "$REPO_ROOT/app/FocusPolicy.swift" \
    "$REPO_ROOT/tests/FocusPolicyVerification.swift" \
    -o "$TEST_TMP_DIR/focus-verification"
"$TEST_TMP_DIR/focus-verification"
