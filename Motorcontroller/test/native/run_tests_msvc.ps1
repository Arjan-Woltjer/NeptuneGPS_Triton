# Run Motorcontroller's native AUnit tests with MSVC, on Windows machines without a
# host GCC (where `pio test -e native` cannot run). The shared runner,
# tools/run_native_tests_msvc.ps1, holds the Visual Studio lookup, AUnit, the
# flags, the test_*.cpp glob and the summary; this file lists what is this
# project's own. Paths are relative to the project folder.
#
# Usage: .\test\native\run_tests_msvc.ps1   (after: pio pkg install -e native)

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

# An uninitialized submodule is an empty directory, which otherwise surfaces as a
# confusing MSVC C1083 "cannot open source file" much further down.
if (-not (Test-Path "$root\lib\SteeringActuator\src\SteeringActuator.cpp")) {
    Write-Error "SteeringActuator sources not found -- the submodule is not checked out. Run: git submodule update --init Motorcontroller/lib/SteeringActuator"
    exit 1
}

$includes = @(
    'lib\MotorcontrollerCore\src',
    'lib\SteeringActuator\src'
)
$sources = @(
    'lib\MotorcontrollerCore\src\InterfaceMotorcontroller.cpp',
    'lib\SteeringActuator\src\SteeringActuator.cpp'
)

& "$root\..\tools\run_native_tests_msvc.ps1" -ProjectRoot $root -Name MotorcontrollerNativeTests -Includes $includes -Sources $sources
exit $LASTEXITCODE
