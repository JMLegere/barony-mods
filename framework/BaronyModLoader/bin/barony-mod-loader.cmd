@echo off
setlocal
set SCRIPT_DIR=%~dp0
set APP_ROOT=%SCRIPT_DIR%..
if /I not "%OS%"=="Windows_NT" (
  echo {"status":"blocked","platform":"windows","reason":"Windows launcher requires Windows_NT shell and verified live Windows runtime evidence."}
  exit /b 2
)
python "%APP_ROOT%\app\barony_mod_loader.py" %*
