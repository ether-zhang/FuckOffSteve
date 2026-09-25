$ErrorActionPreference = 'Stop'
$fosRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (@(Get-Process -Name Mewgenics -ErrorAction SilentlyContinue).Count) {
    throw 'Exit Mewgenics normally before updating. No game process was stopped.'
}
$fosDll = Join-Path $fosRoot 'build\FuckOffSteve.dll'
$fosUi = Join-Path $fosRoot 'build\ui.swf'
if (-not (Test-Path -LiteralPath $fosDll) -or -not (Test-Path -LiteralPath $fosUi)) {
    throw 'Build the standalone mod before applying the update.'
}
$fosLength = (Get-Item -LiteralPath $fosUi).Length
$fosStream = [System.IO.File]::OpenRead($fosUi)
try {
    $fosHeader = New-Object byte[] 8
    if ($fosStream.Read($fosHeader,0,8) -ne 8 -or [System.Text.Encoding]::ASCII.GetString($fosHeader,0,3) -ne 'FWS' -or [BitConverter]::ToUInt32($fosHeader,4) -ne $fosLength) {
        throw 'UI asset must be an uncompressed FWS with its exact declared length. Nothing was installed.'
    }
} finally { $fosStream.Dispose() }
$fosUiDir = Join-Path $fosRoot 'swfs'
New-Item -ItemType Directory -Path $fosUiDir -Force | Out-Null
foreach ($fosPair in @(@($fosUi,(Join-Path $fosUiDir 'ui.swf')),@($fosDll,(Join-Path $fosRoot 'FuckOffSteve.dll')))) {
    $fosHash = (Get-FileHash -LiteralPath $fosPair[0] -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $fosPair[0] -Destination $fosPair[1]
    if ((Get-FileHash -LiteralPath $fosPair[1] -Algorithm SHA256).Hash -ne $fosHash) { throw 'Installed file hash mismatch.' }
}
Write-Output 'FuckOffSteve update VERIFIED: standalone DLL and uncompressed UI asset match the build.'
