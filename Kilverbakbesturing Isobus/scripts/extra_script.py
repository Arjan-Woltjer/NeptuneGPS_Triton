"""
PlatformIO post extra_script — correct RAM overflow check for IMXRT1062 (Teensy 4.1).

ITCM and DTCM are NOT independent 512 KB banks. Per imxrt1062_t41.ld, they
share a single 512 KB / 16-block (32 KB each) FlexRAM pool:
    _itcm_block_count = ceil((itcm code + padding) / 32K)
    _estack = ORIGIN(DTCM) + ((16 - _itcm_block_count) << 15)
DTCM's real usable size is whatever's left after ITCM claims its blocks, so
the correct check is ITCM + DTCM <= 512 KB combined (matching Paul
Stoffregen's explanation on the PJRC forum, thread 73201) -- not each
against 512 KB independently, which would silently pass a build that
overflows the real shared pool (e.g. 491K ITCM + 146K DTCM = 637K, which
fits neither bank's flawed 512K check but exceeds the true combined budget).
"""
Import("env")
import sys, os, re

FLEXRAM_LIMIT = 512 * 1024   # IMXRT1062 FlexRAM pool, shared by ITCM + DTCM


def _imxrt_check(output, flash_limit):
    """Return (ok, error_string) using the correct combined FlexRAM check for IMXRT1062."""
    flash_m = re.search(r'FLASH:\s+code:(\d+),\s+data:(\d+),\s+headers:(\d+)', output)
    ram1_m  = re.search(r'RAM1:\s+variables:(\d+),\s+code:(\d+),\s+padding:(\d+)', output)
    if not flash_m or not ram1_m:
        return True, ""  # unparseable — don't block

    flash_used = int(flash_m.group(1)) + int(flash_m.group(2)) + int(flash_m.group(3))
    itcm_used  = int(ram1_m.group(2)) + int(ram1_m.group(3))   # code + padding
    dtcm_used  = int(ram1_m.group(1))                          # variables
    flexram_used = itcm_used + dtcm_used

    errors = []
    if flash_used > flash_limit:
        errors.append(f"Error FLASH overflow: {flash_used:,} of {flash_limit:,} bytes used")
    if flexram_used > FLEXRAM_LIMIT:
        errors.append(
            f"Error FlexRAM overflow: {flexram_used:,} of {FLEXRAM_LIMIT:,} bytes used "
            f"(ITCM {itcm_used:,} + DTCM {dtcm_used:,} share one 512K pool)"
        )

    if errors:
        return False, "\n".join(errors) + "\n"
    return True, ""


# ── patch exec_command inside the Teensy builder function ───────────────────
# The Teensy function only prints result["err"] when returncode != 0.
# We return empty out/err on success so it stays silent, and our own error
# message in err on genuine overflow so it gets printed before env.Exit(1).
method_wrapper = getattr(env, "CheckUploadSize", None)
if method_wrapper and hasattr(method_wrapper, "method"):
    func = method_wrapper.method
    if "exec_command" in func.__globals__:
        _orig        = func.__globals__["exec_command"]
        _flash_limit = int(env.BoardConfig().get("upload.maximum_size", 8 * 1024 * 1024))

        def _make_patched(orig, flash_limit):
            def _patched(args, **kwargs):
                result = orig(args, **kwargs)
                if args and "teensy_size" in str(args[0]):
                    output = result.get("out", "") + result.get("err", "")
                    ok, errmsg = _imxrt_check(output, flash_limit)
                    return dict(result, returncode=0 if ok else 1, out="", err=errmsg)
                return result
            return _patched

        func.__globals__["exec_command"] = _make_patched(_orig, _flash_limit)

# ── fix SIZEPRINTCMD so the 'size' target applies the same logic ─────────
wrapper = os.path.join(env["PROJECT_DIR"], "scripts", "teensy_size.py")
env.Replace(SIZEPRINTCMD=f'"{sys.executable}" "{wrapper}" $SOURCES')
