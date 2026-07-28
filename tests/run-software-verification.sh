#!/bin/bash

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK_PATH="$(xcrun --show-sdk-path --sdk macosx)"
TEST_TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/codex-remote-tests.XXXXXX")"
trap 'rm -rf "$TEST_TMP_DIR"' EXIT

if [ "$(uname -m)" = "arm64" ]; then
    TARGET="arm64-apple-macosx13.0"
else
    TARGET="x86_64-apple-macosx13.0"
fi

swiftc \
    -sdk "$SDK_PATH" \
    -target "$TARGET" \
    -o "$TEST_TMP_DIR/software-verification" \
    "$REPO_ROOT"/SiriRemoteCore/Sources/SiriRemoteCore/*.swift \
    "$REPO_ROOT/app/KeyMap.swift" \
    "$REPO_ROOT/tests/SoftwareVerificationMain.swift"

"$TEST_TMP_DIR/software-verification" \
    "$REPO_ROOT/examples/codex-remote-v1.jsonc"
