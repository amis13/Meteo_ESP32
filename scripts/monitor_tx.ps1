param(
    [Parameter(Mandatory = $true)]
    [string]$Port
)

. "$PSScriptRoot\common.ps1"

Monitor-Port -Port $Port
