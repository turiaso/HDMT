@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

rem Compilacion reproducible para SGDK en Windows.
rem Uso:
rem   tools\build_sgdk.bat "D:\MDDev\SGDKs\SGDK-master"
rem   tools\build_sgdk.bat "D:\MDDev\SGDKs\SGDK-master" debug
rem   tools\build_sgdk.bat "D:\MDDev\SGDKs\SGDK-master" clean
rem   tools\build_sgdk.bat "D:\MDDev\SGDKs\SGDK-master" debug-clean

if not "%~1"=="" set "GDK_WIN=%~1"
if not defined GDK_WIN (
    echo ERROR: falta la ruta a SGDK.
    echo Uso: tools\build_sgdk.bat "D:\ruta\a\SGDK" [debug^|clean^|debug-clean]
    exit /b 2
)
if not exist "%GDK_WIN%\makefile.gen" (
    echo ERROR: no se encuentra "%GDK_WIN%\makefile.gen".
    exit /b 2
)
if not exist "%GDK_WIN%\bin\make.exe" (
    echo ERROR: no se encuentra "%GDK_WIN%\bin\make.exe".
    echo Comprueba la carpeta de instalacion de SGDK.
    exit /b 2
)

rem makefile.gen y las reglas de recursos usan GDK. No depender de que
rem la variable global del usuario apunte por casualidad a esta instalacion.
set "GDK=%GDK_WIN:\=/%"
set "PATH=%GDK_WIN%\bin;%PATH%"

if /I "%~2"=="clean" goto clean_build
if /I "%~2"=="debug-clean" goto clean_debug_build
goto run_build

:clean_build
echo Limpiando artefactos anteriores de out\...
if exist "out" rmdir /s /q "out"
goto run_build

:clean_debug_build
echo Limpiando artefactos anteriores de out\...
if exist "out" rmdir /s /q "out"

:run_build
echo.
echo SGDK: %GDK_WIN%
echo Perfil: %~2
if /I "%~2"=="debug" goto run_debug
if /I "%~2"=="debug-clean" goto run_debug
"%GDK_WIN%\bin\make.exe" -f "%GDK_WIN%\makefile.gen"
goto check_build

:run_debug
"%GDK_WIN%\bin\make.exe" -f "%GDK_WIN%\makefile.gen" debug

:check_build
if errorlevel 1 (
    echo.
    echo ERROR: fallo la compilacion SGDK. Busca el primer error real anterior
    echo a los mensajes derivados del makefile o del compilador de recursos.
    exit /b 1
)
if not exist "out\rom.bin" (
    echo ERROR: make termino sin generar out\rom.bin.
    exit /b 1
)
echo.
echo ROM generada: %CD%\out\rom.bin
for %%F in ("out\rom.bin") do echo Tamano ROM: %%~zF bytes
exit /b 0
