@echo off
setlocal
set "FOS_ROOT=%~dp0"
set "FOS_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%FOS_VSWHERE%" exit /b 1
for /f "usebackq delims=" %%i in (`"%FOS_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "FOS_VS=%%i"
if not defined FOS_VS exit /b 1
call "%FOS_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not defined FOS_TEST_RUNNER set "FOS_TEST_RUNNER=%FOS_ROOT%build\native_action_tests.exe"
pushd "%FOS_ROOT%build"
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 /I"%FOS_ROOT%vendor\mew-ui-api\src\native" "%FOS_ROOT%tests\test_main.cpp" "%FOS_ROOT%tests\native_args_tests.cpp" "%FOS_ROOT%tests\steve_exit_tests.cpp" "%FOS_ROOT%src\safe_read.cpp" mew_ui_api.obj /Fe:"%FOS_TEST_RUNNER%" /link user32.lib
if errorlevel 1 goto :failed
"%FOS_TEST_RUNNER%"
if errorlevel 1 goto :failed
popd
exit /b 0
:failed
popd
exit /b 1
