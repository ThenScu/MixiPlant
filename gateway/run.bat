@echo off
setlocal
cd /d "%~dp0"

if not exist ".venv\Scripts\python.exe" goto create_venv
if not exist ".env" goto create_env
if /I "%~1"=="--install" goto install_requirements

".venv\Scripts\python.exe" -c "import fastapi, uvicorn, paho.mqtt.client, jose, dotenv" >nul 2>&1
if errorlevel 1 goto install_requirements
goto start_server

:create_venv
where python >nul 2>&1
if not errorlevel 1 (
  python -m venv .venv
  if errorlevel 1 goto setup_failed
) else (
  where py >nul 2>&1
  if errorlevel 1 goto python_missing
  py -3 -m venv .venv
  if errorlevel 1 goto setup_failed
)
if not exist ".venv\Scripts\python.exe" goto setup_failed

:create_env
".venv\Scripts\python.exe" setup_env.py
if errorlevel 1 goto setup_failed

:install_requirements
echo Checking/installing gateway packages. This happens on first run or with --install.
".venv\Scripts\python.exe" -m pip install -r requirements.txt
if errorlevel 1 goto setup_failed

:start_server
echo MixiPlan dashboard: http://127.0.0.1:8000
echo Press Ctrl+C to stop the gateway.
".venv\Scripts\python.exe" -m uvicorn app:app --host 127.0.0.1 --port 8000
exit /b %errorlevel%

:python_missing
echo Python 3 was not found. Install Python and enable "Add Python to PATH", then reopen this terminal.
exit /b 1

:setup_failed
echo Setup failed. Read the error above; to retry package installation, run gateway\run.bat --install from the project root.
exit /b 1
