param([string]$Compiler = 'gcc')

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$buildDir = Join-Path ([IO.Path]::GetTempPath()) ('co3009-tests-' + [Guid]::NewGuid().ToString('N'))
$common = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic', '-ICore/Inc')
$realFsm = @('Core/Src/global.c', 'Core/Src/fsm.c', 'Core/Src/fsm_automatic.c', 'Core/Src/fsm_manual.c', 'Core/Src/fsm_tuning.c')
$targets = @(
    @{ Name = 'api_smoke'; Sources = @('tests/host/api_smoke.c'); Real = $false },
    @{ Name = 'fsm_automatic_test'; Sources = $realFsm + 'tests/host/fsm_automatic_test.c'; Real = $true },
    @{ Name = 'fsm_manual_test'; Sources = @('Core/Src/global.c', 'Core/Src/fsm_manual.c', 'tests/host/fsm_manual_test.c'); Real = $true },
    @{ Name = 'fsm_tuning_test'; Sources = @('Core/Src/global.c', 'Core/Src/fsm_tuning.c', 'tests/host/fsm_tuning_test.c'); Real = $true },
    @{ Name = 'fsm_modes_test'; Sources = $realFsm + 'tests/host/fsm_modes_test.c'; Real = $true }
)

New-Item -ItemType Directory -Path $buildDir | Out-Null
Push-Location -LiteralPath $repoRoot
try {
    foreach ($target in $targets) {
        $sources = @($target.Sources + 'tests/host/mock_modules.c')
        $exe = Join-Path $buildDir ($target.Name + '.exe')
        $arguments = $common
        if ($target.Real) { $arguments += '-DMOCK_MODULES_USE_REAL_FSM' }
        & $Compiler @arguments @sources '-o' $exe
        if ($LASTEXITCODE -ne 0) { throw "Build failed for $($target.Name)." }
        & $exe
        if ($LASTEXITCODE -ne 0) { throw "Tests failed for $($target.Name)." }
    }
} finally {
    Pop-Location
    foreach ($target in $targets) {
        $exe = Join-Path $buildDir ($target.Name + '.exe')
        if (Test-Path -LiteralPath $exe) { Remove-Item -LiteralPath $exe }
    }
    if (@(Get-ChildItem -LiteralPath $buildDir -Force).Count -eq 0) {
        Remove-Item -LiteralPath $buildDir
    }
}
