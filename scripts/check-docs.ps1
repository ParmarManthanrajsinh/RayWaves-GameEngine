param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if (-not $Root) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

$excluded = @(
    [regex]::Escape("$Root\build\"),
    [regex]::Escape("$Root\dist\"),
    [regex]::Escape("$Root\.git\"),
    [regex]::Escape("$Root\third_party\"),
    [regex]::Escape("$Root\.agents\"),
    [regex]::Escape("$Root\Tools\")
)

$staleTokens = @(
    "DemoGame",
    "build_gamelogic.bat",
    "dist_CMakeLists.txt",
    "create_distribution.bat",
    "run_all.bat",
    "run_tests.bat",
    "run_smoketest.bat",
    "Tests/",
    "Distribution/",
    "Documentation/"
)

$errors = [System.Collections.Generic.List[string]]::new()

$files = Get-ChildItem $Root -Recurse -Filter *.md -File | Where-Object {
    $path = $_.FullName
    -not ($excluded | Where-Object { $path -match $_ })
}

foreach ($file in $files) {
    $content = [System.IO.File]::ReadAllText($file.FullName, [System.Text.Encoding]::UTF8)
    $relative = $file.FullName.Substring($Root.Length).TrimStart('\')

    # ADRs intentionally quote historical names when describing past decisions.
    if (-not $relative.StartsWith("docs\adr\")) {
        foreach ($token in $staleTokens) {
            if ($content.Contains($token)) {
                $errors.Add("${relative}: stale token '$token'")
            }
        }
    }

    $fenceCount = ([regex]::Matches($content, '```')).Count
    if (($fenceCount % 2) -ne 0) {
        $errors.Add("${relative}: unbalanced code fence")
    }

    $matches = [regex]::Matches($content, '\[[^\]]+\]\(([^)]+)\)')
    foreach ($match in $matches) {
        $target = $match.Groups[1].Value.Trim()

        if ($target -match '^(https?://|mailto:|#)') { continue }
        if ($target.StartsWith("<") -and $target.EndsWith(">")) {
            $target = $target.Substring(1, $target.Length - 2)
        }

        $target = $target.Split('#')[0]
        if (-not $target) { continue }
        $target = [uri]::UnescapeDataString($target)

        $resolved = Join-Path $file.DirectoryName $target
        if (-not (Test-Path $resolved)) {
            $errors.Add("${relative}: broken link -> $target")
        }
    }
}

if ($errors.Count -gt 0) {
    Write-Host "Documentation check failed:" -ForegroundColor Red
    foreach ($errorItem in $errors) {
        Write-Host "  $errorItem" -ForegroundColor Red
    }
    exit 1
}

Write-Host "Documentation check passed ($($files.Count) files)." -ForegroundColor Green
exit 0
