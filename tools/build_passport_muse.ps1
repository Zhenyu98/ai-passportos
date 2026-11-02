param(
    [string]$IdfRoot,
    [string]$SdkRoot,
    [switch]$PublicAssets
)
$ErrorActionPreference='Stop'
$passportRoot=Split-Path -Parent $PSScriptRoot
$workspaceRoot=Split-Path -Parent $passportRoot
if(-not $IdfRoot) {$IdfRoot=Join-Path $workspaceRoot 'scratch\esp-idf-v6.0.1'}
if(-not $SdkRoot) {$SdkRoot=Join-Path $workspaceRoot 'scratch\muse-gadget-sdk'}
# ESP-IDF's native build tools require paths without spaces on Windows.
Add-Type -Namespace PassportMuse -Name Paths -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet=System.Runtime.InteropServices.CharSet.Unicode, SetLastError=true)]
public static extern uint GetShortPathName(string path, System.Text.StringBuilder output, uint length);
'@
function Get-PassportShortPath([string]$Path) {
    $resolved=(Resolve-Path -LiteralPath $Path).Path
    $buffer=[System.Text.StringBuilder]::new(4096)
    if([PassportMuse.Paths]::GetShortPathName($resolved,$buffer,4096) -eq 0) {throw "Cannot resolve build path: $resolved"}
    return $buffer.ToString()
}
$env:IDF_TOOLS_PATH=Join-Path (Get-PassportShortPath $workspaceRoot) 'scratch\idf6-tools'
# Choose an installed IDF 6 environment rather than the machine's unrelated
# default Python (which may have a different minor version).
$idfEnvironments=@(Get-ChildItem -LiteralPath (Join-Path $env:IDF_TOOLS_PATH 'python_env') -Directory |
    Where-Object { $_.Name -like 'idf6.0_py*_env' -and (Test-Path -LiteralPath (Join-Path $_.FullName 'Scripts\python.exe')) } |
    Sort-Object Name -Descending)
if(-not $idfEnvironments.Count) {throw 'Install the ESP-IDF 6 Python environment first'}
$env:IDF_PYTHON_ENV_PATH=Join-Path (Join-Path $env:IDF_TOOLS_PATH 'python_env') $idfEnvironments[0].Name
$env:PATH=(Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts')+';'+$env:PATH
. (Join-Path (Get-PassportShortPath $IdfRoot) 'export.ps1')
if(-not $env:IDF_PYTHON_ENV_PATH) {throw 'ESP-IDF 6 environment is not installed; follow the Muse integration guide'}
$buildPython=Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
$version=& $buildPython (Join-Path $env:IDF_PATH 'tools\idf.py') --version
if($LASTEXITCODE -ne 0 -or $version -notmatch 'v6\.0\.1') {throw "Expected ESP-IDF v6.0.1; got $version"}
& $buildPython (Join-Path $PSScriptRoot 'prepare_passport_muse.py') --sdk $SdkRoot
if($LASTEXITCODE -ne 0) {throw 'Muse source preparation failed'}
$repoShort=Get-PassportShortPath $passportRoot
$project=Join-Path $repoShort 'muse'
$build=Join-Path $repoShort 'build\muse-firmware'
$assetArgs=@()
if($PublicAssets) {$assetArgs=@('-DPASSPORT_USE_LOCAL_ASSETS=OFF')}
& $buildPython (Join-Path $env:IDF_PATH 'tools\idf.py') -C $project -B $build @assetArgs build
if($LASTEXITCODE -ne 0) {throw 'Muse firmware build failed'}
& $buildPython (Join-Path $env:IDF_PATH 'tools\idf.py') -C $project -B $build merge-bin -o (Join-Path $build 'FoloToy-AI-Passport-full.bin')
if($LASTEXITCODE -ne 0) {throw 'Muse image merge failed'}
& $buildPython (Join-Path $PSScriptRoot 'verify_firmware.py') $build
if($LASTEXITCODE -ne 0) {throw 'Muse firmware verification failed'}
& $buildPython (Join-Path $PSScriptRoot 'archive_firmware.py') create $build --archive-root (Join-Path $passportRoot 'build\firmware')
if($LASTEXITCODE -ne 0) {throw 'Muse firmware archive failed'}
Write-Output "Muse firmware built at $build"
