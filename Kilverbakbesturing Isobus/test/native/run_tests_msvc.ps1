# Run Kilverbakbesturing's native AUnit tests with MSVC, on Windows machines without a
# host GCC (where `pio test -e native` cannot run). The shared runner,
# tools/run_native_tests_msvc.ps1, holds the Visual Studio lookup, AUnit, the
# flags, the test_*.cpp glob and the summary; this file lists what is this
# project's own. Paths are relative to the project folder.
#
# Usage: .\test\native\run_tests_msvc.ps1   (after: pio pkg install -e native)

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

# The real AgIsoStack-plus-plus is compiled for the host, as in Ploegbesturing
# Isobus (#291): three include roots, and sources from isobus/src, utility/src
# and the one portable file in hardware_integration/src. Fetched by
# "pio pkg install -e native".
$includes = @(
    'lib\KilverbakbesturingCore\src',
    '.pio\libdeps\native\AgIsoStack__\isobus\include',
    '.pio\libdeps\native\AgIsoStack__\utility\include',
    '.pio\libdeps\native\AgIsoStack__\hardware_integration\include',
    '..\MeijWorks Libs\IsobusImplement',
    '..\MeijWorks Libs\VehicleGuidance',
    '..\MeijWorks Libs\EepromStore',
    '..\MeijWorks Libs\Interpolation',
    '..\MeijWorks Libs\LcdNumber',
    '..\MeijWorks Libs\TritonText',
    '..\MeijWorks Libs\EndShutoff',
    '..\MeijWorks Libs\ButtonPair'
)
$sources = @(
    'lib\KilverbakbesturingCore\src\implement\ImplementScraper.cpp',
    'lib\KilverbakbesturingCore\src\InterfaceScraper.cpp',
    'lib\KilverbakbesturingCore\src\calibration\CalibrationScraper.cpp',
    'lib\KilverbakbesturingCore\src\calibration\ScraperSettings.cpp',
    'lib\KilverbakbesturingCore\src\isobus\VTObjectPool.cpp',
    'lib\KilverbakbesturingCore\src\isobus\VTImages.generated.cpp',
    'lib\KilverbakbesturingCore\src\isobus\ScraperRefCapture.cpp',
    'lib\KilverbakbesturingCore\src\isobus\ScraperVtWorkingSet.cpp',
    '..\MeijWorks Libs\IsobusImplement\IsobusVtInterface.cpp',
    '..\MeijWorks Libs\IsobusImplement\VtFailoverPolicy.cpp',
    '.pio\libdeps\native\AgIsoStack__\isobus\src\*.cpp',
    '.pio\libdeps\native\AgIsoStack__\utility\src\*.cpp',
    '.pio\libdeps\native\AgIsoStack__\hardware_integration\src\can_hardware_interface.cpp',
    '..\MeijWorks Libs\VehicleGuidance\NmeaParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\TrimbleParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\CanSerialParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\SerialGuidanceChannel.cpp',
    '..\MeijWorks Libs\VehicleGuidance\IsobusPgnDecode.cpp',
    '..\MeijWorks Libs\VehicleGuidance\CanFrameGuidanceChannel.cpp'
)
$defines = @('ISOBUS', 'CAN_STACK_DISABLE_THREADS')

& "$root\..\tools\run_native_tests_msvc.ps1" -ProjectRoot $root -Name KilverbakbesturingIsobusNativeTests -Includes $includes -Sources $sources -Defines $defines
exit $LASTEXITCODE
