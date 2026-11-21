param(
    [string]$Preset = "zig-debug",
    [switch]$Keep
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$RwCli = Join-Path $Root "build/$Preset/rwcli.exe"
if (-not (Test-Path $RwCli)) {
    throw "rwcli not found at $RwCli. Build the rwcli target first."
}

$Temp = Join-Path $env:TEMP ("rwcli-smoke-" + [guid]::NewGuid().ToString("N"))
$Wrapper = Join-Path $Temp ".raywaves/bin/rwcli.cmd"
Write-Host "Agent smoke project: $Temp" -ForegroundColor Cyan
Copy-Item (Join-Path $Root "templates/Simple2D") $Temp -Recurse

# Native stderr becomes an ErrorRecord under EAP=Stop in Windows PowerShell.
# Quiet it only around rwcli calls; the exit code is the real signal.
function Invoke-RwCli {
    param([string[]]$Arguments)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & $RwCli @Arguments 2>$null
        $script:RwCliExit = $LASTEXITCODE
        return $output
    }
    finally {
        $ErrorActionPreference = $previous
    }
}

function Invoke-Wrapper {
    param([string[]]$Arguments)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & $Wrapper @Arguments 2>$null
        $script:RwCliExit = $LASTEXITCODE
        return $output
    }
    finally {
        $ErrorActionPreference = $previous
    }
}

try {
    Write-Host "doctor" -ForegroundColor Cyan
    $doctor = Invoke-RwCli @("doctor", $Temp, "--json")
    if ($script:RwCliExit -ne 0) { throw "doctor failed: $doctor" }

    Write-Host "lsp (generate wrapper)" -ForegroundColor Cyan
    $lspBootstrap = Invoke-RwCli @("lsp", $Temp, "--json")
    if (-not (Test-Path $Wrapper)) { throw "rwcli wrapper missing: $lspBootstrap" }

    Write-Host "build (via wrapper)" -ForegroundColor Cyan
    $build = Invoke-Wrapper @("build", $Temp, "--json")
    $buildJson = $build | ConvertFrom-Json
    if (-not $buildJson.ok) {
        throw "build failed: $($buildJson.rawTail)"
    }

    Write-Host "run (via wrapper)" -ForegroundColor Cyan
    $run = Invoke-Wrapper @("run", $Temp, "--frames", "60", "--screenshot", "shot.png", "--state", "state.json", "--json")
    $runJson = $run | ConvertFrom-Json
    if (-not $runJson.ok) { throw "run failed: $run" }
    if (-not $runJson.screenshot.nonEmpty) { throw "screenshot is blank" }
    if (-not (Test-Path (Join-Path $Temp "state.json"))) { throw "state.json missing" }

    Write-Host "run with input (via wrapper)" -ForegroundColor Cyan
    Set-Content (Join-Path $Temp "input.txt") "0,KEY_RIGHT,down`n30,KEY_RIGHT,up`n"
    $inputRun = Invoke-Wrapper @("run", $Temp, "--frames", "30", "--input", "input.txt", "--json")
    $inputJson = $inputRun | ConvertFrom-Json
    if (-not $inputJson.ok) { throw "input run failed: $inputRun" }
    $player = @($inputJson.entities) | Where-Object { $_.name -eq "Player" } | Select-Object -First 1
    if (-not $player -or $player.position[0] -le 320) { throw "input replay did not move the player" }

    Write-Host "scaffold: templates/new/new-level" -ForegroundColor Cyan
    $templatesJson = (Invoke-RwCli @("templates", "--json")) | ConvertFrom-Json
    if (-not $templatesJson.ok) { throw "templates failed" }
    if ($templatesJson.templates -notcontains "Empty2D") { throw "Empty2D template missing" }

    $Scaffold = Join-Path $Temp "ScaffoldGame"
    $newJson = (Invoke-RwCli @("new", $Scaffold, "--template", "Empty2D", "--json")) | ConvertFrom-Json
    if (-not $newJson.ok) { throw "project scaffold failed" }
    $newLevelJson = (Invoke-RwCli @("new-level", $Scaffold, "ExtraLevel", "--json")) | ConvertFrom-Json
    if (-not $newLevelJson.ok -or -not $newLevelJson.registrationAdded) { throw "new-level failed" }
    if (-not (Test-Path (Join-Path $Scaffold "GameLogic/ExtraLevel.h"))) { throw "level header missing" }
    $scaffoldBuildJson = (Invoke-RwCli @("build", $Scaffold, "--json")) | ConvertFrom-Json
    if (-not $scaffoldBuildJson.ok) { throw "scaffolded project build failed: $($scaffoldBuildJson.rawTail)" }

    Write-Host "scene --type" -ForegroundColor Cyan
    $sceneJson = (Invoke-Wrapper @("scene", "add", $Temp, "--map", "SmokeMap", "--name", "Spawn", "--type", "spawn", "--json")) | ConvertFrom-Json
    if (-not $sceneJson.ok) { throw "scene add --type failed" }
    $sceneText = Get-Content (Join-Path $Temp "Assets/SmokeMap.scene") -Raw
    if ($sceneText -notmatch "type=spawn") { throw "scene type missing from file" }

    Write-Host "validate (via wrapper)" -ForegroundColor Cyan
    $validate = Invoke-Wrapper @("validate", $Temp, "--json")
    $validateJson = $validate | ConvertFrom-Json
    if (-not $validateJson.ok) { throw "validate failed: $validate" }

    $clangd = Get-Command clangd.exe -ErrorAction SilentlyContinue
    if (-not $clangd -and (Test-Path "C:\Program Files\LLVM\bin\clangd.exe")) {
        $clangd = Get-Item "C:\Program Files\LLVM\bin\clangd.exe"
    }
    if ($clangd) {
        Write-Host "lsp --check (via wrapper)" -ForegroundColor Cyan
        $lsp = Invoke-Wrapper @("lsp", $Temp, "--check", "--json")
        $lspJson = $lsp | ConvertFrom-Json
        if (-not $lspJson.ok -or @($lspJson.diagnostics).Count -ne 0) { throw "LSP diagnostics: $lsp" }
    }
    else {
        Write-Warning "clangd not found; skipping LSP check"
    }

    $entityCount = @($runJson.entities).Count
    Write-Host "Agent smoke passed: $entityCount entities, $($runJson.screenshot.bytes) byte screenshot" -ForegroundColor Green
}
finally {
    if (-not $Keep) {
        Remove-Item $Temp -Recurse -Force -ErrorAction SilentlyContinue
    }
}
