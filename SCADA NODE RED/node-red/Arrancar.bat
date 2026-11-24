@echo off
cd /d "%~dp0"

rem Carpeta de usuario de Node-RED (cambiala si usas otra: set USERDIR=C:\ruta\.node-red)
if "%USERDIR%"=="" set "USERDIR=%USERPROFILE%\.node-red"

rem Copia el front del SCADA (uibuilder\scada\src de este paquete) a la carpeta de uibuilder
echo Actualizando uibuilder\scada\src en "%USERDIR%" ...
robocopy "%~dp0..\uibuilder\scada\src" "%USERDIR%\uibuilder\scada\src" /E /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 (
  echo ERROR: no se pudo copiar el front del SCADA. Revisa que exista "%~dp0..\uibuilder\scada\src"
  pause
  exit /b 1
)

node-red "%~dp0flow.json"
pause
