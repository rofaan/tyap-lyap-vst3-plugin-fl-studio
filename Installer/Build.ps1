param(
    [Parameter(Mandatory)][string] $Bundle,
    [Parameter(Mandatory)][string] $Output
)
$ErrorActionPreference = 'Stop'
$bundlePath = (Resolve-Path -LiteralPath $Bundle).Path
$binary = Join-Path $bundlePath 'Contents/x86_64-win/Tyap Lyap.vst3'
$moduleInfo = Join-Path $bundlePath 'Contents/Resources/moduleinfo.json'
if (!(Test-Path -LiteralPath $binary -PathType Leaf)) { throw "Missing VST3 binary: $binary" }
if ((Get-Content -Raw -LiteralPath $moduleInfo) -notmatch '"Version": "2\.2\.0"') {
    throw "The VST3 bundle must be version 2.2.0."
}
$compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$manifest = Join-Path $PSScriptRoot 'Setup.manifest'
$source = Join-Path $PSScriptRoot 'Setup.cs'
$banner = Join-Path $PSScriptRoot 'banner.png'
$options = @(
    '/nologo', '/target:winexe', '/platform:x64', '/optimize+', '/codepage:65001'
    "/out:$Output", "/win32manifest:$manifest"
    '/reference:System.Windows.Forms.dll', '/reference:System.Drawing.dll', "/resource:$binary,TyapLyap.Binary"
    "/resource:$moduleInfo,TyapLyap.ModuleInfo", "/resource:$banner,TyapLyap.Banner", $source
)
& $compiler @options
if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed ($LASTEXITCODE)." }
