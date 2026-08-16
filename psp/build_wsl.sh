#!/bin/bash
# Rebuild EBOOT.PBP inside WSL (pspdev + local libmpc required once).
set -e
export LD_LIBRARY_PATH="${HOME}/local-libs:${LD_LIBRARY_PATH:-}"
export PSPDEV="${HOME}/pspdev"
export PATH="${HOME}/bin:${PSPDEV}/bin:${PATH}"
cd "$(dirname "$0")"
make clean
make -j"$(nproc)"
ls -la EBOOT.PBP
echo "Built: $(pwd)/EBOOT.PBP"
