param([string]$Compiler = 'gcc', [string]$Target)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$buildDir = Join-Path ([IO.Path]::GetTempPath()) ('co3009-tests-' + [Guid]::NewGuid().ToString('N'))
$common = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic', '-ICore/Inc')
$realFsm = @('Core/Src/global.c', 'Core/Src/fsm.c', 'Core/Src/fsm_automatic.c', 'Core/Src/fsm_manual.c', 'Core/Src/fsm_tuning.c')

$targets = @(
    @{ Name = 'api_smoke'; Sources = @('tests/host/api_smoke.c') },
    @{ Name = 'fsm_automatic_test'; Sources = $realFsm + 'tests/host/fsm_automatic_test.c'; Real = $true },
    @{ Name = 'fsm_manual_test'; Sources = @('Core/Src/global.c', 'Core/Src/fsm_manual.c', 'tests/host/fsm_manual_test.c'); Real = $true },
    @{ Name = 'fsm_tuning_test'; Sources = @('Core/Src/global.c', 'Core/Src/fsm_tuning.c', 'tests/host/fsm_tuning_test.c'); Real = $true },
    @{ Name = 'fsm_modes_test'; Sources = $realFsm + 'tests/host/fsm_modes_test.c'; Real = $true },
    @{
        Name = 'scheduler_test'
        Sources = @('Core/Src/scheduler.c', 'tests/host/scheduler_test.c')
        Scheduler = $true
    },
    @{
        Name = 'software_timer_test'
        Sources = @(
            'Core/Src/software_timer.c'
            'Core/Src/scheduler.c'
            'tests/host/software_timer_test.c'
        )
        Scheduler = $true
    }
)

if ($Target) {
    $targets = @($targets | Where-Object { $_.Name -eq $Target })
    if ($targets.Count -eq 0) { throw "Unknown target: $Target" }
}

New-Item -ItemType Directory -Path $buildDir | Out-Null
Push-Location -LiteralPath $repoRoot
try {
    foreach ($entry in $targets) {
        $sources = @($entry.Sources)
        if (-not $entry.Scheduler) { $sources += 'tests/host/mock_modules.c' }
        $exe = Join-Path $buildDir ($entry.Name + '.exe')
        $arguments = $common
        if ($entry.Scheduler) { $arguments += '-DSCH_HOST_TEST' }
        if ($entry.Real) { $arguments += '-DMOCK_MODULES_USE_REAL_FSM' }

        & $Compiler @arguments @sources '-o' $exe
        if ($LASTEXITCODE -ne 0) { throw "Build failed for $($entry.Name)." }

        & $exe
        if ($LASTEXITCODE -ne 0) { throw "Tests failed for $($entry.Name)." }
    }
} finally {
    Pop-Location
    foreach ($entry in $targets) {
        $exe = Join-Path $buildDir ($entry.Name + '.exe')
        if (Test-Path -LiteralPath $exe) { Remove-Item -LiteralPath $exe }
    }
    if (@(Get-ChildItem -LiteralPath $buildDir -Force).Count -eq 0) {
        Remove-Item -LiteralPath $buildDir
    }
}
