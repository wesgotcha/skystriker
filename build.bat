@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

rem ---- SKY STRIKER build script (ASCII only, keep cmd parsing safe) ----
set CXX=g++
set CXXFLAGS=-std=c++17 -O2 -Wall -Wno-unused-parameter -Wno-missing-field-initializers
set INC=-I"src" -I"src/core" -I"src/screens" -I"src/game" -I"raylib-cpp-6.0.3/include" -I"vendor/raylib-6.2_win64_mingw-w64/include"
set LIBDIR=-L"vendor/raylib-6.2_win64_mingw-w64/lib"
set LIBS=-lraylib -lopengl32 -lgdi32 -lwinmm -lshell32

set SRC=
for %%f in (src\*.cpp src\core\*.cpp src\screens\*.cpp src\game\*.cpp) do set SRC=!SRC! %%f

if not exist build mkdir build

echo [1/2] checking chinese glyph coverage...
python tools\gen_charset.py
if errorlevel 1 (
    echo.
    echo [warn] some UI text is missing from the font subset - see report above
)

echo [2/2] compiling...
%CXX% %CXXFLAGS% %INC% %SRC% %LIBDIR% %LIBS% -o build\SkyStriker.exe
if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

echo.
echo BUILD OK: build\SkyStriker.exe
