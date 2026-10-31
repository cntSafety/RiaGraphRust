param(
    [string]$EnvFile
)

$ErrorActionPreference = "Stop"

# Guard: this script is Windows-only by design (build.rs only calls
# it when cfg!(windows) and no sh is available).
if ($env:OS -ne "Windows_NT") {
    Write-Error "download_riag.ps1 is a Windows-only script. Use download_riag.sh on this platform."
    exit 1
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Split-Path -Parent $ScriptDir

# Configuration (env vars with defaults matching the upstream .sh)
$LibKind = if ($env:RIAG_LIB_KIND) { $env:RIAG_LIB_KIND } else { "static" }
$Repository = if ($env:RIAG_GITHUB_REPOSITORY) { $env:RIAG_GITHUB_REPOSITORY } else { "cntSafety/RiaGraph" }
$RunId = if ($env:RIAG_PRECOMPILED_RUN_ID) { $env:RIAG_PRECOMPILED_RUN_ID } else { "" }
$VersionOverride = if ($env:RIAG_VERSION) { $env:RIAG_VERSION } else { "" }
$TargetDir = if ($env:RIAG_TARGET_DIR) { $env:RIAG_TARGET_DIR } else { [System.IO.Path]::Combine($ProjectDir, ".cache", "riag-prebuilt", "lib") }

if ($LibKind -ne "shared" -and $LibKind -ne "static") {
    Write-Error "Unsupported RIAG_LIB_KIND: $LibKind (expected 'shared' or 'static')"
    exit 1
}

$Arch = $env:PROCESSOR_ARCHITECTURE
if ($Arch -eq "AMD64") {
    $Arch = "x86_64"
} elseif ($Arch -eq "ARM64") {
    $Arch = "arm64"
}

# Only x86_64 prebuilt archives are published for Windows.
if ($Arch -ne "x86_64") {
    Write-Error "Unsupported Windows architecture: $Arch (only x86_64 prebuilt archives are published)"
    exit 1
}

if ($LibKind -eq "static") {
    $Archive = "libriag-static-windows-x86_64.zip"
    $ArtifactName = "libriag-static-windows-x86_64"
    $LibName = "riag.lib"
} else {
    $Archive = "libriag-windows-x86_64.zip"
    $ArtifactName = "libriag-windows-x86_64"
    $LibName = "riag_shared.dll"
}

$LibPath = Join-Path $TargetDir $LibName
if (Test-Path $LibPath) {
    Write-Output "libriag already exists in $TargetDir"
    exit 0
}

New-Item -ItemType Directory -Force $TargetDir | Out-Null
$TmpDir = Join-Path ([System.IO.Path]::GetTempPath()) "riag-download-$(Get-Random)"
New-Item -ItemType Directory -Force $TmpDir | Out-Null

try {
    $SourceDesc = ""
    if ($RunId) {
        if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
            Write-Error "gh CLI is required when RIAG_PRECOMPILED_RUN_ID is set"
            exit 1
        }
        gh run download $RunId --repo $Repository --name $ArtifactName --dir (Join-Path $TmpDir "artifact") *>$null
        if ($LASTEXITCODE -ne 0) {
            Write-Error "gh run download failed"
            exit 1
        }
        $ExtractedArchive = Get-ChildItem -Path (Join-Path $TmpDir "artifact") -Recurse -File -Name $Archive | Select-Object -First 1
        if (-not $ExtractedArchive) {
            Write-Error "Artifact ${ArtifactName} does not contain ${Archive}"
            exit 1
        }
        Move-Item -Path ([System.IO.Path]::Combine($TmpDir, "artifact", $ExtractedArchive)) -Destination ([System.IO.Path]::Combine($TmpDir, $Archive)) -Force
        $SourceDesc = "run:${RunId}/${ArtifactName}"
    } else {
        if ($VersionOverride) {
            $Version = $VersionOverride -replace '^v', ''
            $DownloadUrl = "https://github.com/${Repository}/releases/download/v${Version}/${Archive}"
            $SourceDesc = "release:v${Version}"
        } else {
            $DownloadUrl = "https://github.com/${Repository}/releases/latest/download/${Archive}"
            $SourceDesc = "release:latest"
        }
        try {
            Invoke-WebRequest -UseBasicParsing $DownloadUrl -OutFile (Join-Path $TmpDir $Archive)
        } catch {
            $statusCode = try { $_.Exception.Response.StatusCode.value__ } catch { 0 }
            Write-Error "Failed to download ${Archive} from ${DownloadUrl} (HTTP ${statusCode}). Check that the release exists and has this asset."
            exit 1
        }
    }

    $ArchivePath = Join-Path $TmpDir $Archive
    Expand-Archive -Path $ArchivePath -DestinationPath $TargetDir -Force

    Write-Output "Installed ${Archive} from ${SourceDesc} to $TargetDir"
} finally {
    Remove-Item -Recurse -Force $TmpDir -ErrorAction SilentlyContinue
}

if (-not (Test-Path $LibPath)) {
    Write-Error "Expected precompiled library not found at $LibPath"
    exit 1
}

$OutEnvFile = if ($EnvFile) { $EnvFile } else { [System.IO.Path]::Combine($ProjectDir, ".cache", "riag-prebuilt.env") }
$EnvFileDir = Split-Path -Parent $OutEnvFile
if (-not (Test-Path $EnvFileDir)) {
    New-Item -ItemType Directory -Force $EnvFileDir | Out-Null
}
@"
RIAG_LIBRARY_DIR=$TargetDir
RIAG_INCLUDE_DIR=$TargetDir
"@ | Set-Content -Path $OutEnvFile -Encoding utf8

Write-Output "Wrote $OutEnvFile"
Write-Output "Resolved precompiled library: $LibPath"
