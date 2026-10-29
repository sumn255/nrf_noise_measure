@echo off
setlocal
rem Run the portable activity/animation/debounce tests with local Visual Studio.
set "TASK_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%TASK_VSWHERE%" (
    echo Visual Studio C/C++ tools are required for these desktop tests.
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%TASK_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "TASK_VS=%%i"
if not defined TASK_VS (
    echo Visual Studio C/C++ tools were not found.
    exit /b 1
)
call "%TASK_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist build-model-tests mkdir build-model-tests
cl.exe /nologo /W4 /std:c11 /I src tests\test_models.c src\spectrum.c src\button_filter.c /Fe:build-model-tests\test_models.exe /Fo:build-model-tests\ /link /INCREMENTAL:NO
if errorlevel 1 (
    popd
    exit /b 1
)
build-model-tests\test_models.exe
set "TASK_RESULT=%ERRORLEVEL%"
popd
exit /b %TASK_RESULT%
