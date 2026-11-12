param(
    [Parameter(Mandatory)][string] $Previous,
    [Parameter(Mandatory)][string] $Current,
    [Parameter(Mandatory)][string] $Bundle,
    [string] $PreviousVersion = '2.1.0',
    [string] $CurrentVersion = '2.2.0'
)
$ErrorActionPreference = 'Stop'
$testRoot = (Get-Location).Path
$qa = Join-Path $testRoot ('qa-' + [guid]::NewGuid().ToString('N').Substring(0, 6))
$flags = [Reflection.BindingFlags]'Static,NonPublic,Public'
$old = [Reflection.Assembly]::LoadFile((Resolve-Path -LiteralPath $Previous).Path).GetType('Setup')
$new = [Reflection.Assembly]::LoadFile((Resolve-Path -LiteralPath $Current).Path).GetType('Setup')
function Invoke-Setup($type, $method, [string] $argument) {
    $type.GetMethod($method, $flags).Invoke($null, [object[]]@([string] $argument))
}
try {
    New-Item -ItemType Directory -Path $qa | Out-Null
    if (Invoke-Setup $new 'HasInstalledBundle' $qa) { throw 'Fresh install incorrectly detected as update' }
    Invoke-Setup $old 'Install' $qa | Out-Null
    if (!(Invoke-Setup $new 'HasInstalledBundle' $qa)) { throw 'Previous installation not detected' }
    $target = Join-Path $qa 'VST3/Tyap Lyap.vst3'
    $before = Invoke-Setup $new 'InstalledVersion' $target
    if ($before.Trim() -ne $PreviousVersion) { throw "Unexpected previous version: $before" }
    $backup = Invoke-Setup $new 'Install' $qa
    $after = Invoke-Setup $new 'InstalledVersion' $target
    if ($after.Trim() -ne $CurrentVersion) { throw "Unexpected installed version: $after" }
    if ((Invoke-Setup $new 'InstalledVersion' $backup).Trim() -ne $PreviousVersion) { throw 'Backup did not preserve previous version' }
    $relative = 'Contents/x86_64-win/Tyap Lyap.vst3'
    $actual = (Get-FileHash -LiteralPath (Join-Path $target $relative) -Algorithm SHA256).Hash
    $expected = (Get-FileHash -LiteralPath (Join-Path $Bundle $relative) -Algorithm SHA256).Hash
    if ($actual -ne $expected) { throw 'Installed VST3 does not match the tested build' }
    Write-Output "PASS: clean install detection, update $PreviousVersion -> $CurrentVersion, previous-version backup, exact installed binary SHA256"
}
finally {
    # Only remove this run's known workspace child, never any real VST3 installation.
    $resolved = [IO.Path]::GetFullPath($qa)
    if ([IO.Path]::GetDirectoryName($resolved) -ne $testRoot -or [IO.Path]::GetFileName($resolved) -notmatch '^qa-[0-9a-f]{6}$') {
        throw 'Refusing cleanup outside the exact QA workspace child'
    }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
