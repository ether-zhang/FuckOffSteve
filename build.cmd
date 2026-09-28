@echo off
setlocal
set "FOS_ROOT=%~dp0"
set "FOS_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%FOS_VSWHERE%" exit /b 1
for /f "usebackq delims=" %%i in (`"%FOS_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "FOS_VS=%%i"
if not defined FOS_VS exit /b 1
call "%FOS_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "%FOS_ROOT%build" mkdir "%FOS_ROOT%build"
if not defined FOS_PYTHON set "FOS_PYTHON=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
if not exist "%FOS_PYTHON%" set "FOS_PYTHON=python"
"%FOS_PYTHON%" -B "%FOS_ROOT%tools\check_native_api.py"
if errorlevel 1 exit /b 1
"%FOS_PYTHON%" -B "%FOS_ROOT%tools\build_steve_prompt.py"
if errorlevel 1 exit /b 1
"%FOS_PYTHON%" -B "%FOS_ROOT%tests\test_steve_prompt.py"
if errorlevel 1 exit /b 1
pushd "%FOS_ROOT%build"
cl /nologo /MT /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /I"%FOS_ROOT%vendor\mew-ui-api\src\native" /c "%FOS_ROOT%vendor\mew-ui-api\src\native\mew_ui_api.c" /Fo:mew_ui_api.obj
if errorlevel 1 goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 /I"%FOS_ROOT%vendor\mew-ui-api\src\native" /LD "%FOS_ROOT%src\mod.cpp" "%FOS_ROOT%src\native_steve_exit.cpp" "%FOS_ROOT%src\safe_read.cpp" mew_ui_api.obj /Fe:FuckOffSteven.dll /link user32.lib
if errorlevel 1 goto :failed
popd
exit /b 0
:failed
popd
exit /b 1
