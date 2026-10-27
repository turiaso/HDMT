@echo off
setlocal EnableExtensions

rem Build reproducible de la ROM y lanzamiento opcional en BlastEm.
rem Requiere GDK apuntando a la raiz de la instalacion SGDK.

if not defined GDK (
    echo ERROR: GDK no esta definido.
    echo Ejemplo: set "GDK=D:\MDDev\SGDKs\SGDK-master"
    exit /b 2
)
if not exist "%GDK%\makefile.gen" (
    echo ERROR: no existe "%GDK%\makefile.gen".
    echo Comprueba que GDK apunta a la raiz de SGDK.
    exit /b 3
)
where make >nul 2>nul
if errorlevel 1 (
    echo ERROR: no se encuentra make en PATH. Instala/activa las herramientas de SGDK.
    exit /b 4
)

 echo [1/3] Compilando proyecto con SGDK real...
make -f makefile
if errorlevel 1 (
    echo ERROR: ha fallado la compilacion SGDK. Conserva la salida anterior para diagnostico.
    exit /b 10
)

set "ROM="
if exist "out\rom.bin" set "ROM=out\rom.bin"
if not defined ROM if exist "out\rom.gen" set "ROM=out\rom.gen"
if not defined ROM (
    echo ERROR: make termino sin error, pero no se encontro out\rom.bin ni out\rom.gen.
    echo Revisa la configuracion de salida de tu version de SGDK.
    exit /b 11
)
for %%F in ("%ROM%") do if %%~zF LEQ 0 (
    echo ERROR: la ROM generada esta vacia: %ROM%
    exit /b 12
)
echo PASS: ROM generada: %ROM%
for %%F in ("%ROM%") do echo Tamano: %%~zF bytes

echo [2/3] Comprobando emulador...
where blastem >nul 2>nul
if errorlevel 1 (
    echo AVISO: BlastEm no esta en PATH; la compilacion ha terminado, pero no se lanza emulador.
    echo Ejecuta manualmente tu emulador con %ROM%.
    exit /b 0
)

echo [3/3] Lanzando BlastEm...
blastem "%CD%\%ROM%"
exit /b %ERRORLEVEL%
