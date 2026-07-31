"""
PlatformIO post extra_script — correct RAM overflow check for IMXRT1062 (Teensy 4.1).

teensy_size incorrectly adds ITCM (code) and DTCM (variables) together and
compares to 512 KB. These are two independent 512 KB SRAM banks on IMXRT1062.
"""
Import("env")
import sys, os, re

ITCM_LIMIT = 512 * 1024   # IMXRT1062 ITCM bank (code + padding), 512 KB
DTCM_LIMIT = 512 * 1024   # IMXRT1062 DTCM bank (variables), 512 KB


def _imxrt_check(output, flash_limit):
    """Return (ok, error_string) using correct per-bank checks for IMXRT1062."""
    flash_m = re.search(r'FLASH:\s+code:(\d+),\s+data:(\d+),\s+headers:(\d+)', output)
    ram1_m  = re.search(r'RAM1:\s+variables:(\d+),\s+code:(\d+),\s+padding:(\d+)', output)
    if not flash_m or not ram1_m:
        return True, ""  # unparseable — don't block

    flash_used = int(flash_m.group(1)) + int(flash_m.group(2)) + int(flash_m.group(3))
    itcm_used  = int(ram1_m.group(2)) + int(ram1_m.group(3))   # code + padding
    dtcm_used  = int(ram1_m.group(1))                          # variables

    errors = []
    if flash_used > flash_limit:
        errors.append(f"Error FLASH overflow: {flash_used:,} of {flash_limit:,} bytes used")
    if itcm_used > ITCM_LIMIT:
        errors.append(f"Error ITCM overflow: {itcm_used:,} of {ITCM_LIMIT:,} bytes used")
    if dtcm_used > DTCM_LIMIT:
        errors.append(f"Error DTCM overflow: {dtcm_used:,} of {DTCM_LIMIT:,} bytes used")

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
