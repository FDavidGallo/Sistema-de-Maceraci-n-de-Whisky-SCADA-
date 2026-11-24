@echo off
cd /d "%~dp0"

rem === Igual que Arrancar.bat, pero antes levanta el tunel Newt (Pangolin) en otra ventana ===

if "%USERDIR%"=="" set "USERDIR=%USERPROFILE%\.node-red"

rem --- 1) Front del SCADA -> carpeta de uibuilder ---
echo Actualizando uibuilder\scada\src en "%USERDIR%" ...
robocopy "%~dp0..\uibuilder\scada\src" "%USERDIR%\uibuilder\scada\src" /E /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 (
  echo ERROR: no se pudo copiar el front del SCADA. Revisa que exista "%~dp0..\uibuilder\scada\src"
  pause
  exit /b 1
)

rem --- 2) Newt ---
rem Datos de conexion (opcional): crea newt_config.bat al lado de este archivo con estas lineas:
rem    set "PANGOLIN_ENDPOINT=https://tu-pangolin.ejemplo.com"
rem    set "NEWT_ID=tu_id"
rem    set "NEWT_SECRET=tu_secreto"
rem Si ya corriste newt una vez con --id/--secret/--endpoint, queda guardado y no hace falta este archivo.
if exist "%~dp0newt_config.bat" call "%~dp0newt_config.bat"

set "NEWT_EXE="
if exist "%~dp0newt.exe" set "NEWT_EXE=%~dp0newt.exe"
if not defined NEWT_EXE for /f "delims=" %%I in ('where newt 2^>nul') do if not defined NEWT_EXE set "NEWT_EXE=%%I"

if defined NEWT_EXE (
  echo Iniciando Newt: "%NEWT_EXE%"
  start "Newt" cmd /k ""%NEWT_EXE%""
) else (
  echo AVISO: no encontre newt.exe ^(ni al lado de este .bat ni en el PATH^). Sigo sin tunel.
)

rem --- 3) Node-RED ---
node-red "%~dp0flow.json"
pause
