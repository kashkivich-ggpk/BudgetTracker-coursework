param(
    [Parameter(Mandatory = $true)]
    [string]$SmartInstallMakerExe,

    [Parameter(Mandatory = $true)]
    [string]$ProjectFile
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $SmartInstallMakerExe)) {
    throw "Smart Install Maker не найден: $SmartInstallMakerExe"
}
if (-not (Test-Path $ProjectFile)) {
    throw "Файл проекта не найден: $ProjectFile"
}

& $SmartInstallMakerExe /build $ProjectFile
if ($LASTEXITCODE -ne 0) {
    throw "Smart Install Maker завершил сборку с ошибкой."
}

Write-Host "Установщик Smart Install Maker успешно собран."
