@echo off
setlocal
if "%~1"=="" (
  echo Usage: invite_protocol_test.cmd absolute-output-directory
  exit /b 2
)
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
if not exist "%~1" mkdir "%~1"
cl /nologo /std:c++17 /EHsc /O2 /MT /I"C:\e_cat\OPCG STOLEN\fdrive\vcpkg2\installed\x86-windows-static\include" /Fo"%~1\invite_protocol_test.obj" /Fe"%~1\invite_protocol_test.exe" "%~dp0invite_protocol_test.cpp" /link /LIBPATH:"C:\e_cat\OPCG STOLEN\fdrive\vcpkg2\installed\x86-windows-static\lib" libcrypto.lib crypt32.lib ws2_32.lib advapi32.lib user32.lib gdi32.lib bcrypt.lib
if errorlevel 1 exit /b 1
"%~1\invite_protocol_test.exe"
exit /b %errorlevel%
