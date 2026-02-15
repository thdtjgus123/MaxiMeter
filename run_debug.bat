@echo off
setlocal

set EXE=build\MaxiMeter_artefacts\Debug\MaxiMeter.exe
if not exist "%EXE%" (
  echo MaxiMeter executable not found: %EXE%
  echo Build first with: cmake --build build --config Debug --parallel
  exit /b 1
)

start "MaxiMeter" "%EXE%"
endlocal
