@echo off
setlocal

REM Setup VS2026 Community environment for x64
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

echo.
echo Building vrc-fish (Release x64)...
echo.

MSBuild vrc-fish.vcxproj /p:Configuration=Release /p:Platform=x64 /m

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo BUILD FAILED!
    pause
    exit /b 1
)

echo.
echo BUILD SUCCEEDED!
pause
