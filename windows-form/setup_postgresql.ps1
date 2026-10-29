$ErrorActionPreference = "Stop"

trap {
    Write-Host ""
    Write-Host "Setup error:" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host ""
    Read-Host "Press Enter to close"
    exit 1
}

$psql = "C:\Program Files\PostgreSQL\18\bin\psql.exe"
$sqlScript = Join-Path $PSScriptRoot "setup_postgresql.sql"
$driver = "PostgreSQL Unicode(x64)"
$server = "127.0.0.1"
$port = 5432
$database = "budget_tracker"
$appUser = "budget_tracker_app"

if (-not (Test-Path $psql)) {
    throw "psql.exe не найден: $psql"
}

if (-not (Test-Path $sqlScript)) {
    throw "SQL-сценарий не найден: $sqlScript"
}

$alphabet = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789!_-"
$random = [System.Security.Cryptography.RandomNumberGenerator]::Create()
$bytes = New-Object byte[] 24
$random.GetBytes($bytes)
$appPassword = -join ($bytes | ForEach-Object {
    $alphabet[$_ % $alphabet.Length]
})

Write-Host ""
Write-Host "Local PostgreSQL setup for Budget Tracker" -ForegroundColor Cyan
Write-Host "psql will ask for the postgres user password." -ForegroundColor Yellow
Write-Host "The password is entered locally and is not displayed."
Write-Host ""

& $psql `
    --host=$server `
    --port=$port `
    --username=postgres `
    --dbname=postgres `
    --variable="app_password=$appPassword" `
    --file=$sqlScript

if ($LASTEXITCODE -ne 0) {
    throw "PostgreSQL setup failed. Check the postgres password."
}

$connectionString =
    "Driver={$driver};Server=$server;Port=$port;Database=$database;" +
    "Uid=$appUser;Pwd=$appPassword;SSLmode=disable;Pooling=false;"

$connection = New-Object System.Data.Odbc.OdbcConnection($connectionString)
try {
    $connection.Open()
    $command = $connection.CreateCommand()
    $command.CommandText = "SELECT current_database(), current_user"
    $reader = $command.ExecuteReader()
    $reader.Read() | Out-Null
    Write-Host ""
    Write-Host (
        "Connection test passed: database={0}, user={1}" -f
        $reader.GetString(0), $reader.GetString(1)
    ) -ForegroundColor Green
    $reader.Close()

    [Environment]::SetEnvironmentVariable(
        "BUDGETTRACKER_POSTGRES_CONNECTION",
        $connectionString,
        [EnvironmentVariableTarget]::User
    )
    $env:BUDGETTRACKER_POSTGRES_CONNECTION = $connectionString
}
finally {
    if ($reader) { $reader.Dispose() }
    if ($command) { $command.Dispose() }
    $connection.Close()
    $connection.Dispose()
}

Write-Host ""
Write-Host "Connection settings were saved for the current Windows user." -ForegroundColor Green
Write-Host "Restart BudgetTracker.exe. The Profile tab should show PostgreSQL storage."
Write-Host ""
Read-Host "Press Enter to close"
