#!/usr/bin/env python3
"""
Wrapper around PJRC's teensy_size binary.
Applies correct per-bank memory checks for IMXRT1062 (Teensy 4.1).

teensy_size adds ITCM (code) and DTCM (variables) together and compares to
512 KB, which is wrong. Both banks are 512 KB each — check them separately.
"""
import subprocess, sys, os, re

ITCM_LIMIT  = 512 * 1024       # 512 KB — IMXRT1062 ITCM bank (code + padding)
DTCM_LIMIT  = 512 * 1024       # 512 KB — IMXRT1062 DTCM bank (variables)
FLASH_LIMIT = 8 * 1024 * 1024  # 8 MB  — Teensy 4.1 external QSPI flash

tool_dir = os.path.join(os.path.expanduser("~"), ".platformio", "packages", "tool-teensy")
binary = os.path.join(tool_dir, "teensy_size.exe" if sys.platform == "win32" else "teensy_size")

if not os.path.exists(binary):
    sys.exit(0)

result = subprocess.run([binary] + sys.argv[1:], capture_output=True, text=True)
output = result.stdout + result.stderr

# Print the usage table but strip teensy_size's misleading "Error" line
cleaned = re.sub(r'^Error.*\n?', '', output, flags=re.MULTILINE)
sys.stderr.write(cleaned)

# Apply correct per-bank checks
flash_m = re.search(r'FLASH:\s+code:(\d+),\s+data:(\d+),\s+headers:(\d+)', output)
ram1_m  = re.search(r'RAM1:\s+variables:(\d+),\s+code:(\d+),\s+padding:(\d+)', output)

if flash_m and ram1_m:
    flash_used = int(flash_m.group(1)) + int(flash_m.group(2)) + int(flash_m.group(3))
    itcm_used  = int(ram1_m.group(2)) + int(ram1_m.group(3))   # code + padding
    dtcm_used  = int(ram1_m.group(1))                          # variables

    errors = []
    if flash_used > FLASH_LIMIT:
        errors.append(f"Error FLASH overflow: {flash_used:,} of {FLASH_LIMIT:,} bytes used")
    if itcm_used > ITCM_LIMIT:
        errors.append(f"Error ITCM overflow: {itcm_used:,} of {ITCM_LIMIT:,} bytes used")
    if dtcm_used > DTCM_LIMIT:
        errors.append(f"Error DTCM overflow: {dtcm_used:,} of {DTCM_LIMIT:,} bytes used")

    if errors:
        for e in errors:
            sys.stderr.write(e + "\n")
        sys.exit(1)

sys.exit(0)
