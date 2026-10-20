# Run Ploegbesturing Isobus's native AUnit tests with MSVC, on Windows machines without a
# host GCC (where `pio test -e native` cannot run). The shared runner,
# tools/run_native_tests_msvc.ps1, holds the Visual Studio lookup, AUnit, the
# flags, the test_*.cpp glob and the summary; this file lists what is this
# project's own. Paths are relative to the project folder.
#
# Usage: .\test\native\run_tests_msvc.ps1   (after: pio pkg install -e native)

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

# The real AgIsoStack-plus-plus is compiled for the host (NeptuneGPS_Triton#98,
# #189); its core is portable C++17. The tree is nested: three include roots,
# and sources from isobus/src, utility/src and the one portable file in
# hardware_integration/src. Fetched by "pio pkg install -e native".
$includes = @(
    'lib\PloegbesturingCore\src',
    '.pio\libdeps\native\AgIsoStack__\isobus\include',
    '.pio\libdeps\native\AgIsoStack__\utility\include',
    '.pio\libdeps\native\AgIsoStack__\hardware_integration\include',
    '..\MeijWorks Libs\VehicleGuidance',
    '..\MeijWorks Libs\TritonIo',
    '..\MeijWorks Libs\EepromStore',
    '..\MeijWorks Libs\Interpolation',
    '..\MeijWorks Libs\LcdNumber',
    '..\MeijWorks Libs\TritonText',
    '..\MeijWorks Libs\EndShutoff',
    '..\MeijWorks Libs\ButtonPair'
)
$sources = @(
    'lib\PloegbesturingCore\src\isobus\IsobusTcInterface.cpp',
    'lib\PloegbesturingCore\src\isobus\IsobusGuidanceChannel.cpp',
    'lib\PloegbesturingCore\src\isobus\IsobusVtInterface.cpp',
    'lib\PloegbesturingCore\src\isobus\IsobusLightbarChannel.cpp',
    'lib\PloegbesturingCore\src\isobus\AgLeaderLightbarEmulation.cpp',
    'lib\PloegbesturingCore\src\isobus\VtFailoverPolicy.cpp',
    '.pio\libdeps\native\AgIsoStack__\isobus\src\*.cpp',
    '.pio\libdeps\native\AgIsoStack__\utility\src\*.cpp',
    '.pio\libdeps\native\AgIsoStack__\hardware_integration\src\can_hardware_interface.cpp',
    'lib\PloegbesturingCore\src\implement\ImplementPlough.cpp',
    'lib\PloegbesturingCore\src\InterfacePlough.cpp',
    'lib\PloegbesturingCore\src\calibration\CalibrationPlough.cpp',
    'lib\PloegbesturingCore\src\calibration\PloughSettings.cpp',
    'lib\PloegbesturingCore\src\isobus\VTObjectPool.cpp',
    'lib\PloegbesturingCore\src\isobus\VTImages.generated.cpp',
    '..\MeijWorks Libs\VehicleGuidance\NmeaParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\TrimbleParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\CanSerialParser.cpp',
    '..\MeijWorks Libs\VehicleGuidance\SerialGuidanceChannel.cpp',
    '..\MeijWorks Libs\VehicleGuidance\IsobusPgnDecode.cpp',
    '..\MeijWorks Libs\VehicleGuidance\CanFrameGuidanceChannel.cpp'
)
$defines = @('ISOBUS', 'CAN_STACK_DISABLE_THREADS')

& "$root\..\tools\run_native_tests_msvc.ps1" -ProjectRoot $root -Name PloegbesturingNativeTests -Includes $includes -Sources $sources -Defines $defines
exit $LASTEXITCODE
