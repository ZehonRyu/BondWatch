# Local companion for watch + phone + PC. No Docker required.
$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $PSScriptRoot)
Set-Location cloud
$env:DATABASE_URL = "sqlite:///bondwatch.db"
$env:PUBLIC_URL = "http://192.168.1.63:11111"
$env:WATCH_SIM_PORT = "0"
python -m pip install -q fastapi "uvicorn[standard]" pydantic
python -m uvicorn main:app --host 0.0.0.0 --port 11111
