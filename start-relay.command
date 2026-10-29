#!/bin/sh
# Double-click (macOS) or run (Linux) to start the DScord relay. The first
# time, it runs the setup.
cd "$(dirname "$0")" || exit 1
if ! command -v node >/dev/null 2>&1; then
  echo "Node.js is not installed. Get the LTS version from https://nodejs.org and try again."
  printf "Press Enter to close. "; read -r _
  exit 1
fi
node relay/relay.mjs "$@"
echo
printf "The relay has stopped. Press Enter to close. "; read -r _
