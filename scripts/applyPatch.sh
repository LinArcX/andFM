#!/usr/bin/bash
set -e

if [ -n $@ ]; then
  if python3 scripts/extractFiles.py "$@"; then
    echo "Patch Success!"
    ./scripts/build.sh --debug
  else
    echo "Patch Failed!"
  fi
fi
