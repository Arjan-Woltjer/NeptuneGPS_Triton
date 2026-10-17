# Run Pootmachinebesturing's native AUnit tests with MSVC, on Windows machines without a
# host GCC (where `pio test -e native` cannot run). The shared runner,
# tools/run_native_tests_msvc.ps1, holds the Visual Studio lookup, AUnit, the
# flags, the test_*.cpp glob and the summary; this file lists what is this
# project's own. Paths are relative to the project folder.
#
# Usage: .\test\native\run_tests_msvc.ps1   (after: pio pkg install -e native)

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

$includes = @(
    'lib\PootmachinebesturingCore\src',
    '..\MeijWorks Libs\VehicleGuidance',
    '..\MeijWorks Libs\EepromStore',
    '..\MeijWorks Libs\Interpolation',
    '..\MeijWorks Libs\LcdNumber',
    '..\MeijWorks Libs\TritonText',
    '..\MeijWorks Libs\EndShutoff'
)
$sources = @(
    'lib\PootmachinebesturingCore\src\implement\ImplementPlanter.cpp',
    'lib\PootmachinebesturingCore\src\InterfacePlanter.cpp',
    'lib\PootmachinebesturingCore\src\calibration\CalibrationPlanter.cpp'
)

& "$root\..\tools\run_native_tests_msvc.ps1" -ProjectRoot $root -Name PootmachinebesturingNativeTests -Includes $includes -Sources $sources
exit $LASTEXITCODE
