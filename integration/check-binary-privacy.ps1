param(
    [Parameter(Mandatory=$true)][string]$Binary,
    [string]$ProfileRoot = '',
    [switch]$Authoring
)
$ErrorActionPreference = 'Stop'
if (!$ProfileRoot) { $ProfileRoot = Join-Path $PSScriptRoot '../.private' }
# Never emit matched paths or key bytes, including on failure.
try { $bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Binary -ErrorAction Stop)) }
catch { throw 'Cannot read binary for privacy checking. Input path is intentionally suppressed.' }
$latin = [Text.Encoding]::GetEncoding(28591)
$raw = $latin.GetString($bytes)
$texts = @($raw, [Text.Encoding]::Unicode.GetString($bytes))
if ($bytes.Length -gt 1) { $texts += [Text.Encoding]::Unicode.GetString($bytes,1,$bytes.Length-1) }
$absolutePaths = 0
$pdbPaths = 0
foreach ($text in $texts) {
    $absolutePaths += [regex]::Matches($text, '(?i)[A-Z]:[\\/][\p{L}\p{N}_. ()-]+[\\/][\p{L}\p{N}_. ()\\/-]+').Count
    $absolutePaths += [regex]::Matches($text, '\\\\[\p{L}\p{N}_.-]+\\[\p{L}\p{N}_. ()-]+\\[\p{L}\p{N}_. ()\\/-]+').Count
    $pdbPaths += [regex]::Matches($text, '(?i)(?:[A-Z]:[\\/]|\\\\)[\p{L}\p{N}_. ()\\/-]+\.pdb').Count
}
$keyHits = 0
$privateMaterialHits = 0
$privateRawHits = 0
$hybridFile = Join-Path $ProfileRoot 'HpkPrivateProfile.h'
if (Test-Path -LiteralPath $hybridFile) {
    $hybrid = [IO.File]::ReadAllText($hybridFile)
    $arrays = @()
    foreach ($name in @('Encoded','Mask')) {
        $m = [regex]::Match($hybrid, "$name\[206\]\s*=\s*\{([^}]+)\}")
        if (!$m.Success) { throw 'Unrecognized hybrid profile; private-key check cannot proceed.' }
        [byte[]]$a = @([regex]::Matches($m.Groups[1].Value,'0x([0-9A-Fa-f]{2})') | ForEach-Object {[Convert]::ToByte($_.Groups[1].Value,16)})
        if ($a.Length -ne 206) { throw 'Invalid hybrid profile.' }
        $arrays += ,$a
        if ($raw.Contains($latin.GetString($a))) { $privateMaterialHits++ }
    }
    [byte[]]$blob = New-Object byte[] 206
    for ($i=0;$i -lt 206;$i++) { $j=($i*101+17)%206; $blob[$i]=$arrays[0][$j] -bxor $arrays[1][$j] }
    if ($raw.Contains($latin.GetString($blob))) { $privateRawHits++ }
    # Also reject unencoded fragments of the secret scalar, not public coordinates.
    for ($i=0;$i -lt 4;$i++) {
        [byte[]]$chunk = $blob[(142+$i*16)..(157+$i*16)]
        if ($raw.Contains($latin.GetString($chunk))) { $privateRawHits++ }
    }
    [Array]::Clear($blob,0,$blob.Length)
}
$sharesFound = 0
$parts = @()
for ($i = 0; $i -lt 4; $i++) {
    $file = Join-Path $ProfileRoot "parts/part$i.h"
    if (!(Test-Path -LiteralPath $file)) { break }
    $source = [IO.File]::ReadAllText($file)
    $match = [regex]::Match($source, "Part$i\[32\]\s*=\s*\{([^}]+)\}")
    if (!$match.Success) { throw 'Unrecognized private profile; key check cannot proceed.' }
    [byte[]]$part = @([regex]::Matches($match.Groups[1].Value, '0x([0-9A-Fa-f]{2})') | ForEach-Object { [Convert]::ToByte($_.Groups[1].Value,16) })
    if ($part.Length -ne 32) { throw 'Invalid private profile; key check cannot proceed.' }
    $parts += ,$part
    if ($raw.Contains($latin.GetString($part))) { $sharesFound++ }
}
if ($parts.Count -eq 4) {
    [byte[]]$key = New-Object byte[] 32
    for ($i=0; $i -lt 32; $i++) { $key[$i]=$parts[0][$i] -bxor $parts[1][$i] -bxor $parts[2][$i] -bxor $parts[3][$i] }
    $hex = -join ($key | ForEach-Object { $_.ToString('x2') })
    if ($raw.Contains($latin.GetString($key))) { $keyHits++ }
    foreach ($text in $texts) { if ($text.ToLowerInvariant().Contains($hex)) { $keyHits++ } }
    [Array]::Clear($key,0,$key.Length)
    $hex = $null
}
Write-Output ("Binary privacy: absolute-path hits={0}; absolute-PDB hits={1}; full-key hits={2}; embedded shares={3}; key check={4}" -f $absolutePaths,$pdbPaths,$keyHits,$sharesFound, $(if ($parts.Count -eq 4) {'checked'} else {'unavailable'}))
Write-Output ("Hybrid key privacy: raw-private hits={0}; encoded-private arrays={1}; authoring={2}" -f $privateRawHits,$privateMaterialHits,$Authoring.IsPresent)
if ($absolutePaths -gt 0 -or $pdbPaths -gt 0 -or $keyHits -gt 0 -or $sharesFound -gt 0 -or $privateRawHits -gt 0 -or ($Authoring -and $privateMaterialHits -gt 0)) { throw 'Binary privacy check failed. Matched contents are intentionally suppressed.' }
# Passing means no detected literal leakage, not resistance to reverse engineering.
