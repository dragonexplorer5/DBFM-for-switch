<#
PowerShell helper: prints instructions and invokes the MSYS2 bash script if MSYS2 is available.
Run this from a normal PowerShell prompt.
#>
param(
    [string]$Msys2Path = "C:\\msys64",
    [string]$ScriptRelPath = "tools/build_mbedtls_for_switch.sh",
    [string]$ToolchainFile = ""
)

Write-Host "This helper will try to run the MSYS2/bash build script for mbedTLS."
Write-Host "If you installed devkitPro, open the MSYS2 MinGW shell and run the bash script directly."

$bash = Join-Path $Msys2Path 'usr/bin/bash.exe'
if (-Not (Test-Path $bash)) {
    Write-Host "MSYS2 bash not found at $bash. Please run the bash script inside MSYS2 manually:" -ForegroundColor Yellow
    Write-Host "  bash $ScriptRelPath /path/to/your/toolchain.cmake"
    exit 0
}

$scriptFull = Join-Path (Get-Location) $ScriptRelPath
if (-Not (Test-Path $scriptFull)) {
    Write-Host "Script not found: $scriptFull" -ForegroundColor Red
    exit 1
}

if ($ToolchainFile -ne "") {
    & $bash -lc "\"$scriptFull\" \"$ToolchainFile\""
} else {
    & $bash -lc "\"$scriptFull\""
}
