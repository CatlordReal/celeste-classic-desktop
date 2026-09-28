$ErrorActionPreference = 'Stop'
$product = 'Celeste Classic'
$installRoot = Split-Path -Parent $PSCommandPath
$expectedRoot = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'CelesteClassic'
$markerPath = Join-Path $installRoot '.celeste-classic-install.json'
$shortcutPath = Join-Path ([Environment]::GetFolderPath('Programs')) "$product.lnk"

if (-not [string]::Equals([IO.Path]::GetFullPath($installRoot), [IO.Path]::GetFullPath($expectedRoot), [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to remove unexpected folder: $installRoot"
}
if (-not (Test-Path -LiteralPath $markerPath)) {
    throw "Refusing to remove folder without $product install marker: $installRoot"
}
if ((Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json).product -ne $product) {
    throw "Refusing to remove folder owned by another product: $installRoot"
}

if (Test-Path -LiteralPath $shortcutPath) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    if ([string]::Equals($shortcut.TargetPath, (Join-Path $installRoot 'ccleste.exe'), [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $shortcutPath -Force
    }
}

Remove-Item -LiteralPath $installRoot -Recurse -Force
Write-Host "$product removed. Saved progress remains in LocalAppData."
