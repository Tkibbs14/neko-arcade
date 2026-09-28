#!/bin/bash
# One-time: python3-pip + python3-venv from Debian, then CPU-only PyTorch in /root/neko-venv
# (Tyler approved the CPU wheel, ~196 MB from download.pytorch.org, on 2026-09-27).
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get install -y -q --no-install-recommends python3-pip python3-venv > /tmp/apt-py.log 2>&1
echo "apt: $(tail -1 /tmp/apt-py.log)"
[ -x /root/neko-venv/bin/python ] || python3 -m venv /root/neko-venv
/root/neko-venv/bin/pip install -q --index-url https://download.pytorch.org/whl/cpu torch 2>&1 | tail -3
/root/neko-venv/bin/python -c "import torch; print('torch', torch.__version__, 'threads', torch.get_num_threads())"
