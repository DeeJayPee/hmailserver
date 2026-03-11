@echo off
setlocal

set HMS_LIBS=%~1
set OUT_DIR=%~2
set TARGET=%~3
set SCRIPT_DIR=%~dp0

NET STOP hMailServer

xcopy /F /Y "%HMS_LIBS%\openssl-1.1.1u\out64\bin\libcrypto-1_1-x64.dll" "%OUT_DIR%"
if errorlevel 1 exit /b 1

xcopy /F /Y "%HMS_LIBS%\openssl-1.1.1u\out64\bin\libssl-1_1-x64.dll" "%OUT_DIR%"
if errorlevel 1 exit /b 1

xcopy /F /Y "%SCRIPT_DIR%..\..\..\..\libraries\libpq-12.2\x64\*.dll" "%OUT_DIR%"
if errorlevel 1 exit /b 1

"%TARGET%" /Register
if errorlevel 1 exit /b 1
