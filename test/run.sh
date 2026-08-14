#!/usr/bin/env bash
# Builds and runs the VoiceEffect::Run stack-discipline tests against a mock ILuaBase.
# Needs a checkout of garrysmod_common (with submodules) for the Lua headers.
#
#   GMCOMMON=/path/to/garrysmod_common ./test/run.sh
set -euo pipefail

GMCOMMON="${GMCOMMON:-garrysmod_common}"
if [ ! -f "$GMCOMMON/include/GarrysMod/Lua/LuaBase.h" ]; then
	echo "garrysmod_common not found at '$GMCOMMON'." >&2
	echo "Clone it and point GMCOMMON at it:" >&2
	echo "  git clone --recursive https://github.com/danielga/garrysmod_common" >&2
	exit 1
fi

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

g++ -std=c++17 -g -O0 -fsanitize=address,undefined \
	-DGMOD_ALLOW_DEPRECATED \
	-I"$GMCOMMON/include" \
	-I"$GMCOMMON/sourcesdk-minimal/public" \
	-I"$GMCOMMON/sourcesdk-minimal/public/tier0" \
	-I"$GMCOMMON/sourcesdk-minimal/public/tier1" \
	"$(dirname "$0")/test_voice_effect_hook.cpp" -o "$OUT/test_voice_effect_hook"

"$OUT/test_voice_effect_hook"
