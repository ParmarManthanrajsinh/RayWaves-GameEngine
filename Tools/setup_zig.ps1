# setup_zig.ps1 - compatibility shim.
#
# The canonical toolchain setup script is Tools/setup_tools.ps1, which reads
# pinned versions from Tools/toolchain.ini. This shim keeps old docs and
# scripts working while they migrate.
param(
    [switch]$SkipZig = $false,
    [switch]$SkipRcEdit = $false,
    [switch]$SkipNinja = $false,
    [switch]$SkipCMake = $false
)

$forward = @{}
if ($SkipZig) { $forward.SkipZig = $true }
if ($SkipRcEdit) { $forward.SkipRcEdit = $true }
if ($SkipNinja) { $forward.SkipNinja = $true }
if ($SkipCMake) { $forward.SkipCMake = $true }

& (Join-Path $PSScriptRoot "setup_tools.ps1") @forward
exit $LASTEXITCODE
