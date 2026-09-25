@echo off
setlocal

set CLI=E:\STM32CubeCLT_1.18.0\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe
set HEX=E:\stm32_project\demo\shubiao\f407_game\build\Debug\f407_game.hex

"%CLI%" -c port=SWD mode=UR -w "%HEX%" -v -rst

echo.
echo Flash done. Exit code: %ERRORLEVEL%
echo.
pause
