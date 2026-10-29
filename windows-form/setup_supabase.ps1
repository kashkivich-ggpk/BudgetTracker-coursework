$ErrorActionPreference = "Stop"

trap {
    Write-Host ""
    Write-Host "Setup error:" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host ""
    Read-Host "Press Enter to close"
    exit 1
}

$driver = "PostgreSQL Unicode(x64)"
$server = "aws-1-eu-central-1.pooler.supabase.com"
$port = 5432
$database = "postgres"
$user = "postgres.tojewvzfcnrktqhwzzrf"

Write-Host "Supabase setup for Financial Assistant" -ForegroundColor Cyan
Write-Host "Enter the Supabase database password." -ForegroundColor Yellow
$securePassword = Read-Host "Password" -AsSecureString
$passwordPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR(
    $securePassword
)

try {
    $password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR(
        $passwordPointer
    )
}
finally {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordPointer)
}

$connectionString =
    "Driver={$driver};Server=$server;Port=$port;Database=$database;" +
    "Uid=$user;Pwd=$password;SSLmode=require;Pooling=true;"

$connection = New-Object System.Data.Odbc.OdbcConnection($connectionString)
try {
    $connection.Open()
    $command = $connection.CreateCommand()
    $command.CommandText = "SELECT current_database(), current_user"
    $reader = $command.ExecuteReader()
    $reader.Read() | Out-Null
    Write-Host (
        "Connection passed: database={0}, user={1}" -f
        $reader.GetString(0), $reader.GetString(1)
    ) -ForegroundColor Green

    [Environment]::SetEnvironmentVariable(
        "BUDGETTRACKER_POSTGRES_CONNECTION",
        $connectionString,
        [EnvironmentVariableTarget]::User
    )

    $configContents = "PostgreSqlConnection=$connectionString"
    $configLocations = @(
        (Join-Path $PSScriptRoot "Debug\storage.config"),
        (Join-Path $PSScriptRoot "x64\Debug\storage.config"),
        (Join-Path $PSScriptRoot "x64\Release\storage.config")
    )
    foreach ($configPath in $configLocations) {
        $configDirectory = Split-Path $configPath -Parent
        New-Item -ItemType Directory -Path $configDirectory -Force |
            Out-Null
        [System.IO.File]::WriteAllText(
            $configPath,
            $configContents,
            (New-Object System.Text.UTF8Encoding($false))
        )
    }
}
finally {
    if ($reader) { $reader.Dispose() }
    if ($command) { $command.Dispose() }
    $connection.Dispose()
}

Write-Host ""
Write-Host "Supabase connection was saved." -ForegroundColor Green
Write-Host "storage.config was created next to Debug and Release applications."
Write-Host "Restart Financial Assistant."
Write-Host ""
Read-Host "Press Enter to close"
