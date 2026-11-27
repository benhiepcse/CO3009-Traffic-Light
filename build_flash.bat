@echo off
setlocal EnableExtensions DisableDelayedExpansion

if /i "%~1"=="--help" goto help
if not "%~2"=="" goto invalid_args
if not "%~1"=="" if /i not "%~1"=="--build-only" goto invalid_args

pushd "%~dp0"
if errorlevel 1 exit /b 1

where /q arm-none-eabi-gcc.exe
if errorlevel 1 (
    echo [ERROR] Add the Arm GNU Toolchain bin folder to PATH.
    goto fail
)

if /i not "%~1"=="--build-only" (
    where /q STM32_Programmer_CLI.exe
    if errorlevel 1 (
        echo [ERROR] Add the STM32CubeProgrammer bin folder to PATH.
        goto fail
    )
)

set "OUT=Debug/vscode"
set "ELF=%OUT%/BTL.elf"
set "HAL=Drivers/STM32F1xx_HAL_Driver"
set "CMSIS=Drivers/CMSIS"
if not exist "Debug\vscode\." mkdir "Debug\vscode"
if not exist "Debug\vscode\." goto fail

rem Relative paths also work with GCC in a workspace containing Unicode.
(
    for %%F in (Core\Src\*.c) do echo Core/Src/%%~nxF
    for %%F in (Core\Startup\*.s) do echo Core/Startup/%%~nxF
    for %%F in (Drivers\STM32F1xx_HAL_Driver\Src\*.c) do echo %HAL%/Src/%%~nxF
) > "%OUT%/sources.rsp"
if errorlevel 1 goto fail

echo [BUILD] STM32F103RB firmware
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -std=gnu11 -Os -g3 ^
    -DSTM32F103xB -DUSE_HAL_DRIVER -ffunction-sections -fdata-sections ^
    -ICore/Inc "-I%HAL%/Inc" "-I%HAL%/Inc/Legacy" "-I%CMSIS%/Include" ^
    "-I%CMSIS%/Device/ST/STM32F1xx/Include" "@%OUT%/sources.rsp" ^
    -TSTM32F103RBTX_FLASH.ld --specs=nano.specs --specs=nosys.specs ^
    -Wl,--gc-sections -Wl,-Map,%OUT%/BTL.map -o "%ELF%"
if errorlevel 1 goto fail
if not exist "%ELF%" goto fail
echo [OK] Built %ELF%
if /i "%~1"=="--build-only" goto success

echo [FLASH] ST-LINK via SWD
STM32_Programmer_CLI.exe -c port=SWD -d "%ELF%" -v -rst
if errorlevel 1 goto fail
echo [OK] Firmware programmed, verified and reset.

:success
popd
exit /b 0

:fail
echo [ERROR] Operation failed. Check the output above.
popd
exit /b 1

:invalid_args
echo Usage: build_flash.bat [--build-only]
exit /b 2

:help
echo Usage: build_flash.bat [--build-only]
echo Build BTL firmware, then program, verify and reset via ST-LINK.
echo Use --build-only to create Debug/vscode/BTL.elf without connecting to hardware.
exit /b 0
