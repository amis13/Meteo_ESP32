param(
    [Parameter(Mandatory = $true)]
    [string]$Mac
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$NormalizedMac = $Mac.ToUpperInvariant()
if ($NormalizedMac -notmatch '^([0-9A-F]{2}:){5}[0-9A-F]{2}$') {
    throw "MAC invalida: $Mac"
}

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$TargetFile = Join-Path $ProjectRoot "esp32_bme_tx\esp32_bme_tx.ino"
$Bytes = $NormalizedMac.Split(":") | ForEach-Object { "0x$_" }
$MacArray = [string]::Join(", ", $Bytes)
$Pattern = '^uint8_t RECEIVER_MAC\[6\] = \{.*\};$'
$Replacement = "uint8_t RECEIVER_MAC[6] = {$MacArray};"
$Content = Get-Content -Path $TargetFile -Raw

if ($Content -notmatch $Pattern) {
    throw "No encuentro RECEIVER_MAC en $TargetFile"
}

$Updated = [regex]::Replace($Content, $Pattern, $Replacement, [System.Text.RegularExpressions.RegexOptions]::Multiline)
[System.IO.File]::WriteAllText($TargetFile, $Updated, [System.Text.Encoding]::ASCII)

Write-Host "MAC del receptor actualizada a $NormalizedMac"
Select-String -Path $TargetFile -Pattern '^uint8_t RECEIVER_MAC\[6\] = \{' | ForEach-Object { $_.Line }
