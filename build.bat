@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

rem ---- SKY STRIKER build script (ASCII only, keep cmd parsing safe) ----
set CXX=g++
set CXXFLAGS=-std=c++17 -O2 -Wall -Wno-unused-parameter -Wno-missing-field-initializers
set INC=-I"src" -I"src/core" -I"src/screens" -I"src/game" -I"raylib-cpp-6.0.3/include" -I"vendor/raylib-6.2_win64_mingw-w64/include"
set LIBDIR=-L"vendor/raylib-6.2_win64_mingw-w64/lib"
rem -mwindows: link as GUI subsystem, so double-click does not spawn a console box.
rem launched from cmd/run.bat the game attaches to the parent console itself, so
rem printf/TraceLog stay visible; use --console to force a window when debugging.
set LIBS=-lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lws2_32 -mwindows

set SRC=
for %%f in (src\*.cpp src\core\*.cpp src\screens\*.cpp src\game\*.cpp) do set SRC=!SRC! %%f

if not exist build mkdir build

echo [1/3] checking chinese glyph coverage...
python tools\gen_charset.py
if errorlevel 1 (
    echo.
    echo [warn] some UI text is missing from the font subset - see report above
)

echo [2/3] preparing model assets...
rem strip unused embedded glTF textures + pre-resize skin maps (see tools/prep_assets.py).
rem optional: if it fails the game falls back to raw models, just slower.
python tools\prep_assets.py
if errorlevel 1 (
    echo.
    echo [warn] asset prep failed - falling back to raw models ^(slower startup^)
)

rem close a running instance, otherwise the linker cannot overwrite the exe
taskkill /IM SkyStriker.exe /F >nul 2>&1
if not errorlevel 1 (
    echo [i] closed running SkyStriker.exe
    ping -n 2 127.0.0.1 >nul
)

echo [3/3] compiling...
rem exe may still be locked for a moment after taskkill - retry a few times
set TRY=0
:link
%CXX% %CXXFLAGS% %INC% %SRC% %LIBDIR% %LIBS% -o build\SkyStriker.exe
if not errorlevel 1 goto linked
set /a TRY+=1
if %TRY% GEQ 4 (
    echo.
    echo BUILD FAILED
    exit /b 1
)
echo [i] link retry %TRY% ...
taskkill /IM SkyStriker.exe /F >nul 2>&1
ping -n 2 127.0.0.1 >nul
goto link
:linked

echo.
echo BUILD OK: build\SkyStriker.exe
