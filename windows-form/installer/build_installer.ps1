$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot -Parent
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"
$iscc = Join-Path $root ".tools\InnoSetup\ISCC.exe"
$redistSource = "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Redist\MSVC\v145\vc_redist.x64.exe"
$redistTarget = Join-Path $PSScriptRoot "redist\vc_redist.x64.exe"

if (-not (Test-Path $msbuild)) {
    throw "MSBuild не найден: $msbuild"
}
if (-not (Test-Path $iscc)) {
    throw "Inno Setup compiler не найден: $iscc"
}
if (-not (Test-Path $redistSource)) {
    throw "VC++ Redistributable не найден: $redistSource"
}

New-Item -ItemType Directory -Path (Split-Path $redistTarget) -Force |
    Out-Null
Copy-Item $redistSource $redistTarget -Force

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
    throw "Release x64 build failed."
}

& $iscc (Join-Path $PSScriptRoot "FinancialAssistant.iss")
if ($LASTEXITCODE -ne 0) {
    throw "Installer build failed."
}

Write-Host ""
Write-Host "Installer created:" -ForegroundColor Green
Write-Host (Join-Path $PSScriptRoot "output\FinancialAssistantSetup.exe")
