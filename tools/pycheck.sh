#!/bin/bash
# What does training a tiny model need here? Python, venv/pip, an NVIDIA GPU visible in WSL, and the
# size of the CPU-only PyTorch wheel.
python3 --version
python3 -c "import venv, ensurepip; print('venv+ensurepip: ok')" 2>&1 | tail -1
python3 -m pip --version 2>&1 | tail -1
python3 -c "import torch; print('torch', torch.__version__)" 2>&1 | tail -1
python3 -c "import numpy; print('numpy', numpy.__version__)" 2>&1 | tail -1
command -v nvidia-smi >/dev/null && nvidia-smi --query-gpu=name,memory.total --format=csv,noheader 2>&1 | head -2 || echo "nvidia-smi: not in WSL PATH"
ls /usr/lib/wsl/lib/libcuda.so* 2>/dev/null | head -2 || true
apt-cache show python3-venv python3-pip 2>/dev/null | grep -E '^(Package|Size|Installed-Size)' | paste - - -
PYV=$(python3 -c 'import sys; print(f"cp{sys.version_info.major}{sys.version_info.minor}")')
curl -s https://download.pytorch.org/whl/cpu/torch/ | grep -oE "torch-[0-9.]+%2Bcpu-${PYV}-${PYV}-manylinux[^\"#]*x86_64\.whl" | sort -V | tail -1 > /tmp/whl.txt
W=$(cat /tmp/whl.txt)
echo "newest cpu wheel for $PYV: $W"
[ -n "$W" ] && curl -sI "https://download.pytorch.org/whl/cpu/$W" | grep -i content-length
