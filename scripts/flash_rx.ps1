param(
    [Parameter(Mandatory = $true)]
    [string]$Port
)

. "$PSScriptRoot\common.ps1"

Compile-Sketch -SketchDir "esp32_oled_rx"
Upload-Sketch -SketchDir "esp32_oled_rx" -Port $Port

Write-Host "Receptor grabado en $Port"
