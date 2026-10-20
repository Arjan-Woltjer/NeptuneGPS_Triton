# Run Spuitcomputer LD's native AUnit tests with MSVC, on Windows machines without a
# host GCC (where `pio test -e native` cannot run). The shared runner,
# tools/run_native_tests_msvc.ps1, holds the Visual Studio lookup, AUnit, the
# flags, the test_*.cpp glob and the summary; this file lists what is this
# project's own. Paths are relative to the project folder.
#
# Usage: .\test\native\run_tests_msvc.ps1   (after: pio pkg install -e native)

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

$includes = @(
    'lib\SpuitcomputerLdCore\src',
    '..\MeijWorks Libs\VehicleGuidance'
)
$sources = @(
    'lib\SpuitcomputerLdCore\src\InterfaceSprayer.cpp',
    'lib\SpuitcomputerLdCore\src\implement\ImplementSprayer.cpp',
    'lib\SpuitcomputerLdCore\src\config\ConfigSprayer.cpp',
    'lib\SpuitcomputerLdCore\src\remote\RemoteSprayer.cpp',
    'lib\SpuitcomputerLdCore\src\calibration\CalibrationSprayer.cpp',
    '..\MeijWorks Libs\VehicleGuidance\NmeaParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\TrimbleParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\CanSerialParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\SerialGuidanceChannel.cpp',
    '..\MeijWorks Libs\VehicleGuidance\IsobusPgnDecode.cpp',
    '..\MeijWorks Libs\VehicleGuidance\CanFrameGuidanceChannel.cpp'
)

& "$root\..\tools\run_native_tests_msvc.ps1" -ProjectRoot $root -Name SpuitcomputerLdNativeTests -Includes $includes -Sources $sources
exit $LASTEXITCODE
