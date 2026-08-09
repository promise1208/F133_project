#!/bin/bash
make clean && make PLATFORM=thead/c910  SUNXI_CHIP=sun20iw1p1 PLATFORM_RISCV_ISA=rv64gcxthead
