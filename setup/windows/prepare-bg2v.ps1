param(
    [string]$ApkPath,
    [string]$OutputRoot,
    [switch]$NoOpen
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Windows.Forms

$mainObb = 'main.5826.com.beamdog.baldursgateIIenhancededition.obb'
$patchObb = 'patch.5826.com.beamdog.baldursgateIIenhancededition.obb'
$requiredEntries = @(
    'lib/armeabi-v7a/libBaldursGate.so',
    "assets/$mainObb",
    "assets/$patchObb"
)
$startupMovies = @('logo.wbm', 'intro.wbm', 'intro15f.wbm')

function Select-Bg2Apk {
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Title = 'Select your Baldur''s Gate II Android APK'
    $dialog.Filter = 'Android APK (*.apk)|*.apk|All files (*.*)|*.*'
    $dialog.Multiselect = $false

    if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
        return $null
    }

    return $dialog.FileName
}

function Copy-ZipEntry {
    param(
        [System.IO.Compression.ZipArchiveEntry]$Entry,
        [string]$Destination
    )

    $parent = Split-Path -Parent $Destination
    if ($parent) {
        [System.IO.Directory]::CreateDirectory($parent) | Out-Null
    }

    $source = $Entry.Open()
    try {
        $target = [System.IO.File]::Create($Destination)
        try {
            $source.CopyTo($target, 1048576)
        }
        finally {
            $target.Dispose()
        }
    }
    finally {
        $source.Dispose()
    }
}

function Find-Ffmpeg {
    $local = Join-Path $PSScriptRoot 'ffmpeg.exe'
    if ([System.IO.File]::Exists($local)) {
        return $local
    }

    $command = Get-Command ffmpeg.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    return $null
}

function Write-DefaultBaldurLua {
    param([string]$Destination)

    $content = @"
SetPrivateProfileString('Program Options','Maximum Frame Rate','30')
SetPrivateProfileString('MOVIES','LOGO','0')
SetPrivateProfileString('MOVIES','INTRO','0')
SetPrivateProfileString('MOVIES','INTRO15F','0')
"@

    [System.IO.File]::WriteAllText($Destination, $content)
}

function Optimize-StartupMovies {
    param(
        [string]$PatchObbPath,
        [string]$DestinationRoot,
        [string]$FfmpegPath
    )

    $moviesDir = Join-Path $DestinationRoot 'movies'
    [System.IO.Directory]::CreateDirectory($moviesDir) | Out-Null

    $temporaryDir = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("bg2v-movies-" + [System.Guid]::NewGuid().ToString('N'))
    [System.IO.Directory]::CreateDirectory($temporaryDir) | Out-Null

    try {
        $obbArchive = [System.IO.Compression.ZipFile]::OpenRead($PatchObbPath)
        try {
            foreach ($movie in $startupMovies) {
                $entry = $obbArchive.GetEntry("movies/$movie")
                if (-not $entry) {
                    throw "Patch OBB is missing movies/$movie"
                }

                $sourceMovie = Join-Path $temporaryDir $movie
                $outputMovie = Join-Path $moviesDir $movie
                Copy-ZipEntry $entry $sourceMovie

                $arguments = @(
                    '-hide_banner',
                    '-loglevel', 'warning',
                    '-y',
                    '-i', $sourceMovie,
                    '-map', '0:v:0',
                    '-map', '0:a?',
                    '-vf', 'scale=640:360:flags=lanczos,fps=15',
                    '-c:v', 'libvpx',
                    '-b:v', '500k',
                    '-crf', '20',
                    '-deadline', 'good',
                    '-cpu-used', '4',
                    '-c:a', 'copy',
                    $outputMovie
                )

                $process = Start-Process -FilePath $FfmpegPath `
                    -ArgumentList $arguments `
                    -NoNewWindow `
                    -PassThru `
                    -Wait
                if ($process.ExitCode -ne 0) {
                    throw "ffmpeg failed while converting $movie"
                }
            }
        }
        finally {
            $obbArchive.Dispose()
        }
    }
    finally {
        if ([System.IO.Directory]::Exists($temporaryDir)) {
            Remove-Item -LiteralPath $temporaryDir -Recurse -Force
        }
    }
}

try {
    if (-not $ApkPath) {
        $ApkPath = Select-Bg2Apk
    }

    if (-not $ApkPath) {
        Write-Host 'Setup cancelled.'
        exit 0
    }

    $ApkPath = [System.IO.Path]::GetFullPath($ApkPath)
    if (-not [System.IO.File]::Exists($ApkPath)) {
        throw "APK not found: $ApkPath"
    }

    if (-not $OutputRoot) {
        $OutputRoot = Join-Path $PSScriptRoot 'BG2V_READY'
        if ([System.IO.Directory]::Exists($OutputRoot)) {
            $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
            $OutputRoot = Join-Path $PSScriptRoot "BG2V_READY-$stamp"
        }
    }

    $OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
    $vitaData = Join-Path $OutputRoot 'bg2v'
    $ffmpegPath = Find-Ffmpeg
    [System.IO.Directory]::CreateDirectory($vitaData) | Out-Null

    Write-Host ''
    Write-Host 'BG2V Setup'
    Write-Host '==========='
    Write-Host "APK: $ApkPath"
    Write-Host ''

    $archive = [System.IO.Compression.ZipFile]::OpenRead($ApkPath)
    try {
        $entries = @{}
        foreach ($entry in $archive.Entries) {
            $entries[$entry.FullName] = $entry
        }

        foreach ($required in $requiredEntries) {
            if (-not $entries.ContainsKey($required)) {
                throw "This APK is not compatible: missing $required"
            }
        }

        Write-Host '[1/4] Extracting the Vita-compatible game library...'
        Copy-ZipEntry $entries['lib/armeabi-v7a/libBaldursGate.so'] `
            (Join-Path $vitaData 'libBaldursGate.so')

        Write-Host '[2/4] Extracting the main game data...'
        Copy-ZipEntry $entries["assets/$mainObb"] (Join-Path $vitaData $mainObb)

        Write-Host '[3/4] Extracting the patch game data...'
        Copy-ZipEntry $entries["assets/$patchObb"] (Join-Path $vitaData $patchObb)

        Write-Host '[4/4] Copying additional APK assets...'
        $assetsRoot = Join-Path $vitaData 'assets'
        $assetsRootFull = [System.IO.Path]::GetFullPath($assetsRoot) + `
            [System.IO.Path]::DirectorySeparatorChar

        foreach ($entry in $archive.Entries) {
            if (-not $entry.FullName.StartsWith('assets/')) {
                continue
            }
            if ($entry.FullName.EndsWith('/')) {
                continue
            }
            if ($entry.FullName -eq "assets/$mainObb" -or
                $entry.FullName -eq "assets/$patchObb") {
                continue
            }

            $relative = $entry.FullName.Substring('assets/'.Length).Replace(
                '/', [System.IO.Path]::DirectorySeparatorChar)
            $destination = [System.IO.Path]::GetFullPath(
                (Join-Path $assetsRoot $relative))
            if (-not $destination.StartsWith(
                    $assetsRootFull,
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Unsafe asset path in APK: $($entry.FullName)"
            }
            Copy-ZipEntry $entry $destination
        }
    }
    finally {
        $archive.Dispose()
    }

    [System.IO.File]::Copy($ApkPath, (Join-Path $vitaData 'game.apk'), $true)

    $movieSummary = 'Startup videos will be skipped by default.'
    if ($ffmpegPath) {
        Write-Host '[extra] Optimizing startup movies for PS Vita...'
        Optimize-StartupMovies `
            -PatchObbPath (Join-Path $vitaData $patchObb) `
            -DestinationRoot $vitaData `
            -FfmpegPath $ffmpegPath
        $movieSummary = 'Startup videos were optimized for PS Vita.'
    } else {
        Write-Host '[extra] ffmpeg not found; disabling startup videos to avoid stutter.'
        Write-DefaultBaldurLua (Join-Path $vitaData 'Baldur.lua')
    }

    $instructions = @"
BG2V data is ready.

1. Open VitaShell on the Vita and press SELECT to start FTP.
2. On the PC, connect to the FTP address displayed by VitaShell.
3. Copy the folder named bg2v into ux0:data/ on the Vita.
4. Install BG2v0_beta.vpk with VitaShell.
5. Launch BG2VBETA from the Vita home screen.

$movieSummary
"@
    [System.IO.File]::WriteAllText(
        (Join-Path $OutputRoot 'COPY_TO_VITA.txt'), $instructions)

    Write-Host ''
    Write-Host 'Success! Your Vita data folder is ready:' -ForegroundColor Green
    Write-Host $vitaData -ForegroundColor Cyan
    Write-Host ''
    Write-Host 'Copy the bg2v folder into ux0:data/ on the Vita.'

    if (-not $NoOpen) {
        Start-Process explorer.exe -ArgumentList ('"{0}"' -f $OutputRoot)
    }
}
catch {
    $message = $_.Exception.Message
    Write-Host ''
    Write-Host "Setup failed: $message" -ForegroundColor Red
    [System.Windows.Forms.MessageBox]::Show(
        $message,
        'BG2V Setup failed',
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Error) | Out-Null
    exit 1
}
