$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot -Parent
$releaseDirectory = Join-Path $root "x64\Release"
$applicationPath = Join-Path $releaseDirectory "BudgetTracker.exe"
$configurationPath = Join-Path $releaseDirectory "storage.config"
$identityPath = Join-Path $env:LOCALAPPDATA "FinancialAssistant\client_id.txt"

function Pass([string] $message) {
    Write-Host "[PASS] $message" -ForegroundColor Green
}

function Fail([string] $message) {
    throw "[FAIL] $message"
}

if (-not [Environment]::Is64BitOperatingSystem) {
    Fail "64-bit Windows is required."
}
Pass "64-bit operating system"

if (-not (Test-Path $applicationPath)) {
    Fail "Release EXE was not found: $applicationPath"
}
Pass "Release EXE exists"

if (-not (Test-Path $configurationPath)) {
    Fail "storage.config was not found next to Release EXE."
}

$configuration = @{}
foreach ($line in [IO.File]::ReadAllLines($configurationPath)) {
    $separator = $line.IndexOf("=")
    if ($separator -gt 0) {
        $configuration[$line.Substring(0, $separator)] =
            $line.Substring($separator + 1)
    }
}
if (-not $configuration.SupabaseUrl -or
    -not $configuration.SupabasePublishableKey) {
    Fail "Supabase Data API configuration is incomplete."
}
if (-not $configuration.SupabasePublishableKey.StartsWith("sb_publishable_")) {
    Fail "A non-publishable Supabase key was found in storage.config."
}
Pass "Only public Supabase Data API configuration is packaged"

if (Test-Path $identityPath) {
    $clientId = [IO.File]::ReadAllText($identityPath).Trim()
    $parsedId = [guid]::Empty
    if (-not [guid]::TryParse($clientId, [ref] $parsedId)) {
        Fail "client_id has an invalid format."
    }
    Pass "Individual client_id exists and is valid"
}
else {
    Write-Host (
        "[INFO] client_id does not exist yet. The app creates it on first run."
    ) -ForegroundColor Cyan
}

$process = Start-Process -FilePath $applicationPath -PassThru
Start-Sleep -Seconds 5
$process.Refresh()
if ($process.HasExited) {
    Fail "Release application exited during startup."
}
if (-not $process.Responding) {
    Fail "Release application is not responding."
}
Pass "Release application started and is responding"

Stop-Process -Id $process.Id -Force
Write-Host ""
Write-Host "Smoke test completed successfully." -ForegroundColor Green
