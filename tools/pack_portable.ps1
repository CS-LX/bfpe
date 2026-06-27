param(
    [string]$BfpeExe = "build-native/bin/bfpe.exe",
    [string]$OutDir = "dist/bfpe-portable",
    [string]$ZipPath = "dist/bfpe-portable.zip"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

function Resolve-RepoPath([string]$RelativePath) {
    return Join-Path $root $RelativePath
}

$bfpeSource = Resolve-RepoPath $BfpeExe
if (-not (Test-Path $bfpeSource)) {
    $found = Get-ChildItem (Resolve-RepoPath "build-native") -Recurse -Filter "bfpe.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($found) {
        $bfpeSource = $found.FullName
    } else {
        throw "bfpe.exe not found (expected $BfpeExe)"
    }
}

$portableRoot = Resolve-RepoPath $OutDir
$zipOutput = Resolve-RepoPath $ZipPath

if (Test-Path $portableRoot) {
    Remove-Item $portableRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $portableRoot, (Join-Path $portableRoot "tools") | Out-Null

Copy-Item $bfpeSource (Join-Path $portableRoot "bfpe.exe")
Copy-Item -Recurse (Resolve-RepoPath "runtime") (Join-Path $portableRoot "runtime")
Copy-Item (Resolve-RepoPath "tools/verify_pe.ps1") (Join-Path $portableRoot "tools/verify_pe.ps1")

@"
BFPE portable layout
====================

Keep bfpe.exe and runtime/ in the same folder, then run:

  .\bfpe.exe build examples\add.bf -o add.dll

Build runs built-in PE verification (no PowerShell required).
tools/verify_pe.ps1 is included for manual cross-check only.

Requires Visual Studio 2022 with C++ tools (ml64, cl, link).
"@ | Set-Content -Encoding UTF8 (Join-Path $portableRoot "README-portable.txt")

if (Test-Path $zipOutput) {
    Remove-Item $zipOutput -Force
}
Compress-Archive -Path (Join-Path $portableRoot "*") -DestinationPath $zipOutput

Write-Host "Packed $zipOutput"
Write-Host "bfpe.exe from $bfpeSource"
