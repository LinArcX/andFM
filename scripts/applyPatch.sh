#!/usr/bin/bash
set -e

if [ -n $@ ]; then
  if python3 scripts/extractFiles.py "$@"; then
    echo "Patch Success!"
    # TODO: --debug and --arm64_v8a should come as parameters.
    ./scripts/build.sh --debug --arm64_v8a
  else
    echo "Patch Failed!"
  fi
fi
