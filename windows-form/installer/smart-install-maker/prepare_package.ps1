param()

$ErrorActionPreference = "Stop"

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"
$releaseDirectory = Join-Path $root "x64\Release"
$packageDirectory = Join-Path $PSScriptRoot "package"
$redistSource = Join-Path $root "installer\redist\vc_redist.x64.exe"

if (-not (Test-Path $msbuild)) {
    throw "MSBuild не найден: $msbuild"
}
if (-not (Test-Path $redistSource)) {
    throw "VC++ Redistributable не найден: $redistSource"
}

[Environment]::SetEnvironmentVariable(
    "PATH",
    $null,
    [EnvironmentVariableTarget]::Process
)

& $msbuild `
    (Join-Path $root "BudgetTracker.sln") `
    /m `
    /p:Configuration=Release `
    /p:Platform=x64 `
    /v:minimal
if ($LASTEXITCODE -ne 0) {
    throw "Не удалось собрать Release x64."
}

New-Item -ItemType Directory -Path $packageDirectory -Force | Out-Null
Copy-Item (Join-Path $releaseDirectory "BudgetTracker.exe") $packageDirectory -Force
Copy-Item (Join-Path $releaseDirectory "storage.config") $packageDirectory -Force
Copy-Item $redistSource $packageDirectory -Force

Write-Host "Комплектация Smart Install Maker подготовлена:"
Write-Host $packageDirectory

