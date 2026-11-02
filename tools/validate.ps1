param(
    [ValidateSet('All','Static','Firmware')][string]$Mode='All',
    [string]$BuildDirectory,
    [string]$PrivateAssetsDirectory,
    [string]$HostPython=$env:PASSPORT_HOST_PYTHON,
    [string]$BashExecutable=$env:PASSPORT_BASH
)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
function Invoke-Checked([string]$Executable,[string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Validation command failed ($LASTEXITCODE): $Executable" }
}
Push-Location $repoRoot
try {
    if ($Mode -ne 'Firmware') {
        if (-not $BashExecutable) {
            $gitPath=(Get-Command git).Source
            $gitBash=Join-Path (Split-Path -Parent $gitPath) '../bin/bash.exe'
            $BashExecutable=if (Test-Path -LiteralPath $gitBash) { $gitBash } else { 'bash' }
        }
        $oldHostPython=$env:PASSPORT_HOST_PYTHON
        $oldBash=$env:PASSPORT_BASH
        try {
            if ($HostPython) { $env:PASSPORT_HOST_PYTHON=$HostPython }
            $env:PASSPORT_BASH=$BashExecutable
            Invoke-Checked $BashExecutable @('./tools/validate.sh','--static')
        } finally { $env:PASSPORT_HOST_PYTHON=$oldHostPython; $env:PASSPORT_BASH=$oldBash }
    }
    if ($Mode -ne 'Static') {
        if (-not $env:IDF_PATH) { throw 'Activate ESP-IDF 5.5.3 first.' }
        $idfPython=(Get-Command python).Source
        $idfScript=Join-Path $env:IDF_PATH 'tools/idf.py'
        if (-not $BuildDirectory) {
            $BuildDirectory=Join-Path ([IO.Path]::GetTempPath()) ('passport-firmware-'+[guid]::NewGuid().ToString('N'))
        }
        $BuildDirectory=[IO.Path]::GetFullPath($BuildDirectory)
        if (Test-Path -LiteralPath $BuildDirectory) { throw 'Choose a new isolated build directory.' }
        $oldMsystem=$env:MSYSTEM
        $oldDefaults=$env:SDKCONFIG_DEFAULTS
        try {
            Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
            $version=& $idfPython $idfScript --version
            if ($LASTEXITCODE -ne 0 -or "$version" -notmatch 'ESP-IDF v5\.5\.3') { throw 'ESP-IDF 5.5.3 is required.' }
            Write-Output $version
            $env:SDKCONFIG_DEFAULTS=Join-Path $repoRoot 'sdkconfig.defaults'
            $buildArgs=@($idfScript,'-B',$BuildDirectory,'-D',("SDKCONFIG="+(Join-Path $BuildDirectory 'sdkconfig')))
            if ($PrivateAssetsDirectory) {
                $assetPath=[IO.Path]::GetFullPath($PrivateAssetsDirectory)
                $buildArgs+=@('-D',("PASSPORT_PRIVATE_ASSETS_DIR="+$assetPath))
            }
            Invoke-Checked $idfPython ($buildArgs+@('build'))
            $image=Join-Path $BuildDirectory 'FoloToy-AI-Passport-full.bin'
            Invoke-Checked $idfPython @($idfScript,'-B',$BuildDirectory,'merge-bin','-o',$image)
            Invoke-Checked $idfPython @('tools/verify_firmware.py',$BuildDirectory)
            Invoke-Checked $idfPython @('tools/archive_firmware.py','create',$BuildDirectory,'--archive-root',(Join-Path $repoRoot 'build/firmware'))
            Copy-Item -LiteralPath $image -Destination (Join-Path $repoRoot 'build/FoloToy-AI-Passport-full.bin')
            Write-Output 'Firmware build: PASS (native Windows; build and symbols retained)'
        } finally {
            $env:MSYSTEM=$oldMsystem
            $env:SDKCONFIG_DEFAULTS=$oldDefaults
        }
    }
} finally { Pop-Location }
