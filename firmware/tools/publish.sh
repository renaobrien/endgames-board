#!/bin/bash
# Publish a console firmware build over the air.
# Usage: bash ~/Desktop/Vibecoding/endgames-board/firmware/tools/publish.sh 0.11.8
set -e
V=${1:?Usage: publish.sh <version>}
FW=~/Desktop/Vibecoding/endgames-board
SITE=~/Desktop/Vibecoding/Endgam.es
BIN=$FW/firmware/release/endgames-board-$V-update.bin
[ -f "$BIN" ] || { echo "Missing $BIN"; exit 1; }

# 1. Save the firmware source in its own repo
cd "$FW"
rm -f .git/HEAD.lock .git/index.lock .git/objects/maintenance.lock
git add -A firmware docs/screenshots
git diff --cached --quiet || git commit -m "firmware $V"
git push origin HEAD 2>/dev/null || echo "(firmware repo has no remote push; saved locally)"

# 2. Put the update on endgam.es and point board.json at it
cd "$SITE"
git fetch origin
TMP=$(mktemp -d)/eg-publish
git worktree add --detach "$TMP" origin/main
cp "$BIN" "$TMP/public/firmware/endgames-board-$V.bin"
printf '{"version": "%s", "url": "https://endgam.es/firmware/endgames-board-%s.bin"}\n' "$V" "$V" > "$TMP/public/firmware/board.json"
cd "$TMP"
git add public/firmware
git commit -m "Console firmware $V"
git push origin HEAD:main
cd "$SITE"
git worktree remove "$TMP"
echo
echo "Published $V. Netlify deploys in 1 to 2 minutes. Restart the console to update now."
