[CmdletBinding()]
param(
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$product = 'Celeste Classic'
$sourceRoot = $PSScriptRoot
$installRoot = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'CelesteClassic'
$markerName = '.celeste-classic-install.json'
$markerPath = Join-Path $installRoot $markerName
$shortcutPath = Join-Path ([Environment]::GetFolderPath('Programs')) "$product.lnk"

foreach ($requiredPath in @('ccleste.exe', 'ccleste-game.exe', 'data', 'gamecontrollerdb.txt', 'records', 'player\mpv.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot $requiredPath))) {
        throw "Installer payload is incomplete: $requiredPath is missing."
    }
}

if (Test-Path -LiteralPath $installRoot) {
    $knownInstall = $false
    if (Test-Path -LiteralPath $markerPath) {
        try {
            $knownInstall = ((Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json).product -eq $product)
        } catch {
            $knownInstall = $false
        }
    }
    if (-not $knownInstall) {
        throw "Refusing to modify unknown folder: $installRoot"
    }
    if (-not $Force) {
        throw "$product is already installed. Use the uninstall command in README.md, or rerun the installer with -Force to replace this known installation."
    }
}

if (Test-Path -LiteralPath $shortcutPath) {
    $shell = New-Object -ComObject WScript.Shell
    $existingShortcut = $shell.CreateShortcut($shortcutPath)
    $expectedTarget = Join-Path $installRoot 'ccleste.exe'
    if (-not [string]::Equals($existingShortcut.TargetPath, $expectedTarget, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to replace shortcut owned by another application: $shortcutPath"
    }
}

$stagingRoot = "$installRoot.staging-$PID"
$backupRoot = "$installRoot.backup-$([guid]::NewGuid().ToString('N'))"
$previousInstallMoved = $false
$newInstallMoved = $false
$stagingCreated = $false
if (Test-Path -LiteralPath $stagingRoot) {
    throw "Refusing to use existing staging folder: $stagingRoot"
}
New-Item -ItemType Directory -Path $stagingRoot | Out-Null
$stagingCreated = $true

try {
    foreach ($payloadItem in Get-ChildItem -LiteralPath $sourceRoot -Force) {
        Copy-Item -LiteralPath $payloadItem.FullName -Destination $stagingRoot -Recurse -Force
    }
    $marker = [ordered]@{ product = $product; installedAt = [DateTime]::UtcNow.ToString('o') } | ConvertTo-Json
    Set-Content -LiteralPath (Join-Path $stagingRoot $markerName) -Value $marker -Encoding UTF8

    if (Test-Path -LiteralPath $installRoot) {
        Move-Item -LiteralPath $installRoot -Destination $backupRoot
        $previousInstallMoved = $true
    }
    Move-Item -LiteralPath $stagingRoot -Destination $installRoot
    $newInstallMoved = $true

    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = Join-Path $installRoot 'ccleste.exe'
    $shortcut.WorkingDirectory = $installRoot
    $shortcut.Description = $product
    $shortcut.Save()

    Write-Host "$product installed for current user at $installRoot"
} catch {
    if ($newInstallMoved -and (Test-Path -LiteralPath $installRoot) -and (Test-Path -LiteralPath $markerPath) -and
        ((Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json).product -eq $product)) {
        if (-not $previousInstallMoved -and (Test-Path -LiteralPath $shortcutPath)) {
            $shell = New-Object -ComObject WScript.Shell
            $shortcut = $shell.CreateShortcut($shortcutPath)
            if ([string]::Equals($shortcut.TargetPath, (Join-Path $installRoot 'ccleste.exe'), [StringComparison]::OrdinalIgnoreCase)) {
                Remove-Item -LiteralPath $shortcutPath -Force
            }
        }
        Remove-Item -LiteralPath $installRoot -Recurse -Force
    }
    if ($previousInstallMoved -and (Test-Path -LiteralPath $backupRoot)) {
        Move-Item -LiteralPath $backupRoot -Destination $installRoot
    }
    throw
} finally {
    if ($stagingCreated -and (Test-Path -LiteralPath $stagingRoot)) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}
if (Test-Path -LiteralPath $backupRoot) {
    try {
        Remove-Item -LiteralPath $backupRoot -Recurse -Force
    } catch {
        Write-Warning "Old install backup remains at $backupRoot; new install is ready."
    }
}
