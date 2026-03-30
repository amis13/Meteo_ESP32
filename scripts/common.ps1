Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$DefaultFqbn = "esp32:esp32:esp32"
$DefaultBaudrate = "115200"

function Get-ArduinoCliPath {
    if ($env:ARDUINO_CLI_BIN -and (Test-Path $env:ARDUINO_CLI_BIN)) {
        return $env:ARDUINO_CLI_BIN
    }

    $command = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($command) {
        if ($command.Path) {
            return $command.Path
        }

        return $command.Source
    }

    foreach ($candidate in @(
        "C:\tools\arduino-cli\arduino-cli.exe",
        "C:\Tools\arduino-cli\arduino-cli.exe"
    )) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "No encuentro arduino-cli en PATH."
}

$script:ArduinoCliBin = Get-ArduinoCliPath
$script:ArduinoFqbn = if ($env:ARDUINO_FQBN) { $env:ARDUINO_FQBN } else { $DefaultFqbn }
$script:MonitorBaudrate = if ($env:MONITOR_BAUDRATE) { $env:MONITOR_BAUDRATE } else { $DefaultBaudrate }

function Invoke-ArduinoCli {
    param(
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $script:ArduinoCliBin @Arguments
}

function Get-BoardList {
    Invoke-ArduinoCli board list
}

function Compile-Sketch {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SketchDir
    )

    $args = @("compile", "--fqbn", $script:ArduinoFqbn)
    if ($env:ARDUINO_LIBRARIES) {
        $args += @("--libraries", $env:ARDUINO_LIBRARIES)
    }

    $args += (Join-Path $ProjectRoot $SketchDir)
    Invoke-ArduinoCli @args
}

function Upload-Sketch {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SketchDir,
        [Parameter(Mandatory = $true)]
        [string]$Port
    )

    Invoke-ArduinoCli upload -p $Port --fqbn $script:ArduinoFqbn (Join-Path $ProjectRoot $SketchDir)
}

function Monitor-Port {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Port
    )

    Invoke-ArduinoCli monitor -p $Port -c "baudrate=$script:MonitorBaudrate,dtr=off,rts=off"
}
