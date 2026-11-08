"""
PlatformIO post extra_script -- run AgIsoStack's rarely used code from flash.

On the Teensy 4, all code that is not FLASHMEM runs from ITCM, which the core
takes in 32 KB blocks out of the same 512 KB that holds the variables and the
stack. One byte over a block boundary costs the stack a whole block: the move
to upstream AgIsoStack-plus-plus 02f3fe9 grew the code by 6.5 KB, crossed a
boundary and took free stack from 75.6 to 42.8 KB.

Our own rarely used code is FLASHMEM already (#192). AgIsoStack's is not, and
marking it would mean patching the library again. Every function is compiled
into its own section (-ffunction-sections), so the linker can place it by
name instead: this script takes the core's own linker script, inserts the
section patterns from flash_cold_code.ld right after its *(.flashmem*) line
(the flash code section), and links with that copy. ld gives each section to
the first statement that matches it, so these come before ITCM's catch-all
*(.text*). Taking the core's script every build keeps it in step with the core.

Only on the IMXRT1062 (Teensy 4.0 and 4.1); every other board is left alone.
"""
Import("env")
import os

ANCHOR = "*(.flashmem*)"


def _fail(message):
    print("flash_cold_code.py: " + message)
    env.Exit(1)


if env.BoardConfig().get("build.mcu", "") == "imxrt1062":
    flags = env["LINKFLAGS"]
    if "-T" not in flags:
        _fail("no -T linker script on the link line")
    index = list(flags).index("-T") + 1
    core_script = env.subst(flags[index])
    if not os.path.isabs(core_script):
        # The link line names the script only; ld looks it up on the -L path.
        for directory in env.get("LIBPATH", []):
            candidate = os.path.join(env.subst(directory), core_script)
            if os.path.isfile(candidate):
                core_script = candidate
                break
        else:
            _fail("cannot find " + core_script + " on the library path")
    with open(core_script, "r", encoding="utf-8") as handle:
        text = handle.read()
    if text.count(ANCHOR) != 1:
        _fail("expected one " + ANCHOR + " in " + core_script)

    fragment_path = os.path.join(env["PROJECT_DIR"], "scripts", "flash_cold_code.ld")
    with open(fragment_path, "r", encoding="utf-8") as handle:
        fragment = handle.read()

    generated = text.replace(ANCHOR, ANCHOR + "\n" + fragment, 1)
    build_dir = env.subst("$BUILD_DIR")
    os.makedirs(build_dir, exist_ok=True)
    out_path = os.path.join(build_dir, "flash_cold_code_" + os.path.basename(core_script))
    old = None
    if os.path.exists(out_path):
        with open(out_path, "r", encoding="utf-8") as handle:
            old = handle.read()
    if old != generated:
        with open(out_path, "w", encoding="utf-8") as handle:
            handle.write(generated)

    flags[index] = out_path
    env.Depends(os.path.join(build_dir, env.subst("${PROGNAME}.elf")), [out_path, fragment_path])
