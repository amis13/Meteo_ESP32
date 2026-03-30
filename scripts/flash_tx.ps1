param(
    [Parameter(Mandatory = $true)]
    [string]$Port
)

. "$PSScriptRoot\common.ps1"

Compile-Sketch -SketchDir "esp32_bme_tx"
Upload-Sketch -SketchDir "esp32_bme_tx" -Port $Port

Write-Host "Emisor grabado en $Port"
