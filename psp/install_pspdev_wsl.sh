#!/bin/bash
set -euo pipefail
mkdir -p "$HOME/bin" "$HOME/pspdev"
cd /tmp

if [ ! -x "$HOME/bin/make" ]; then
  wget -q -O make.deb http://archive.ubuntu.com/ubuntu/pool/main/m/make-dfsg/make_4.3-4.1build1_amd64.deb
  rm -rf make_extract
  mkdir make_extract
  dpkg-deb -x make.deb make_extract
  cp make_extract/usr/bin/make "$HOME/bin/make"
  chmod +x "$HOME/bin/make"
fi
"$HOME/bin/make" --version | head -1

if [ ! -x "$HOME/pspdev/bin/psp-config" ]; then
  echo "Downloading pspdev..."
  wget -O pspdev.tar.gz https://github.com/pspdev/pspdev/releases/latest/download/pspdev-ubuntu-latest-x86_64.tar.gz
  echo "Extracting..."
  tar -xzf pspdev.tar.gz -C "$HOME"
fi

export PSPDEV="$HOME/pspdev"
export PATH="$HOME/bin:$PSPDEV/bin:$PATH"
psp-config --pspsdk-path
psp-gcc --version | head -1
echo "TOOLCHAIN_OK"
