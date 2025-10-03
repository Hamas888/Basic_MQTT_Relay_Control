# Exit on error
$ErrorActionPreference = "Stop"

# Resolve paths
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$ServerDir = Join-Path $ProjectRoot "server"
$AppDir = Join-Path $ServerDir "app"
$VenvActivate = Join-Path $ServerDir "venv\Scripts\Activate.ps1"
$MosquittoExe = "C:\Program Files\Mosquitto\mosquitto.exe"
$MosquittoConf = Join-Path $ServerDir "mosquitto.conf"
$LogFormatter = Join-Path $ServerDir "mosquitto_log_format.py"
$LoggingConfig = Join-Path $ServerDir "logging.yaml"

# Stop any running Mosquitto or Python (FastAPI) processes
Write-Host "Stopping existing Python and Mosquitto processes..."
Get-Process python -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Get-Process mosquitto -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

# Clear terminal
Clear-Host

# Activate virtual environment
Write-Host "Activating virtual environment..."
& $VenvActivate

# Start Mosquitto with config
Write-Host "Starting Mosquitto..."
$global:MosquittoProc = Start-Process `
    -FilePath $MosquittoExe `
    -ArgumentList "-c `"$MosquittoConf`"" `
    -RedirectStandardError "mosq_error.log" `
    -NoNewWindow `
    -PassThru

Write-Host "Mosquitto started with PID $($MosquittoProc.Id)"

# Define cleanup function on exit
Register-EngineEvent PowerShell.Exiting -Action {
    if ($global:MosquittoProc -and !$global:MosquittoProc.HasExited) {
        Write-Host "Stopping Mosquitto (PID $($global:MosquittoProc.Id))..."
        try {
            $global:MosquittoProc.Kill()
        } catch {
            Write-Host "Error while stopping Mosquitto: $_"
        }
    }
    Write-Host "Clean exit completed"
}

# Optionally: stream logs from mosq_error.log using tail-like behavior
Start-Job {
    $logFile = "mosq_error.log"
    if (Test-Path $logFile) {
        Get-Content $logFile -Wait
    }
} | Out-Null

# Start FastAPI (Uvicorn) server
Write-Host "Starting FastAPI (Uvicorn) server..."
uvicorn app.main:app `
    --host 127.0.0.1 `
    --port 8000 `
    --reload `
    --log-config "$LoggingConfig" `
    --app-dir "$ServerDir"
