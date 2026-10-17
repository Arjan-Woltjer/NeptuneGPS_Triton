# Run Kilverbakbesturing's native AUnit tests with MSVC, on Windows machines without a
# host GCC (where `pio test -e native` cannot run). The shared runner,
# tools/run_native_tests_msvc.ps1, holds the Visual Studio lookup, AUnit, the
# flags, the test_*.cpp glob and the summary; this file lists what is this
# project's own. Paths are relative to the project folder.
#
# Usage: .\test\native\run_tests_msvc.ps1   (after: pio pkg install -e native)

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

$includes = @(
    'lib\KilverbakbesturingCore\src',
    '..\MeijWorks Libs\VehicleGuidance',
    '..\MeijWorks Libs\EepromStore',
    '..\MeijWorks Libs\Interpolation',
    '..\MeijWorks Libs\LcdNumber'
)
$sources = @(
    'lib\KilverbakbesturingCore\src\implement\ImplementScraper.cpp',
    'lib\KilverbakbesturingCore\src\InterfaceScraper.cpp',
    'lib\KilverbakbesturingCore\src\calibration\CalibrationScraper.cpp',
    '..\MeijWorks Libs\VehicleGuidance\NmeaParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\TrimbleParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\CanSerialParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\SerialGuidanceChannel.cpp',
    '..\MeijWorks Libs\VehicleGuidance\IsobusPgnDecode.cpp',
    '..\MeijWorks Libs\VehicleGuidance\CanFrameGuidanceChannel.cpp'
)

& "$root\..\tools\run_native_tests_msvc.ps1" -ProjectRoot $root -Name KilverbakbesturingNativeTests -Includes $includes -Sources $sources
exit $LASTEXITCODE
