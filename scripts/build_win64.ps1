param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$preset = if ($Config -eq "Debug") { "win64-debug" } else { "win64-release" }

Write-Host "=== Configuring $preset ===" -ForegroundColor Cyan
cmake --preset $preset

Write-Host "=== Building $preset ($Config) ===" -ForegroundColor Cyan
cmake --build "build/$preset" --config $Config

Write-Host "=== Testing $preset ($Config) ===" -ForegroundColor Cyan
ctest --test-dir "build/$preset" -C $Config --output-on-failure
