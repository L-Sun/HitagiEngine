param(
    [string]$ArchiveName = 'hitagi-technical-blogs-v3.zip',
    [string[]]$Articles = @('gfx-abstraction-refactoring.md', 'cpp-modules-build-optimization.md'),
    [switch]$ChineseOnly
)
$ErrorActionPreference = 'Stop'
$blogRoot = $PSScriptRoot
$repoRoot = [IO.Path]::GetFullPath((Join-Path $blogRoot '../..'))
$outputRoot = Join-Path $repoRoot 'temp/blog-affine'
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$zipPath = Join-Path $outputRoot $ArchiveName
if (Test-Path -LiteralPath $zipPath) {
    throw 'Import archive already exists; preserve it and choose a new output name.'
}

$referencedAssets = [Collections.Generic.HashSet[string]]::new()
Add-Type -AssemblyName System.IO.Compression
$zip = [IO.Compression.ZipFile]::Open($zipPath, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($article in $articles) {
        $source = [IO.File]::ReadAllText((Join-Path $blogRoot $article))
        foreach ($assetMatch in [regex]::Matches($source, '!\[[^\]]*\]\((assets/[^)]+)\)')) {
            $referencedAssets.Add($assetMatch.Groups[1].Value) | Out-Null
        }
        if (([regex]::Matches($source, '(?m)^```')).Count % 2 -ne 0) { throw "Unclosed code fence: $article" }
        # AFFiNE cannot resolve repository-relative source links. Preserve the
        # citations as readable repository paths, while leaving web/image links intact.
        $converted = [regex]::Replace($source, '(?<!!)\[([^\]]+)\]\(((?:\.\./|data/)[^)]+)\)', {
            param($match)
            $relative = $match.Groups[2].Value
            $resolved = [IO.Path]::GetFullPath((Join-Path $blogRoot $relative))
            if (-not (Test-Path -LiteralPath $resolved)) { throw "Missing local reference: $relative" }
            $display = [IO.Path]::GetRelativePath($repoRoot, $resolved).Replace('\', '/')
            return '(' + $match.Groups[1].Value + ': ' + [char]96 + $display + [char]96 + ')'
        })
        $entry = $zip.CreateEntry($article)
        $writer = [IO.StreamWriter]::new($entry.Open(), [Text.UTF8Encoding]::new($false))
        try { $writer.Write($converted) } finally { $writer.Dispose() }
    }
    foreach ($asset in $referencedAssets) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, (Join-Path $blogRoot $asset), $asset) | Out-Null
    }
} finally { $zip.Dispose() }

$readback = [IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $entries = @($readback.Entries.FullName)
    if ($entries.Count -ne ($Articles.Count + $referencedAssets.Count)) { throw 'Unexpected archive entry count.' }
    foreach ($article in $articles) {
        $reader = [IO.StreamReader]::new($readback.GetEntry($article).Open())
        try { $body = $reader.ReadToEnd() } finally { $reader.Dispose() }
        foreach ($image in [regex]::Matches($body, '!\[[^\]]*\]\(([^)]+)\)')) {
            if ($image.Groups[1].Value -notin $entries) { throw 'Missing image in archive.' }
        }
        if ($ChineseOnly) {
            if ($body.Contains('## English')) { throw 'Unexpected English edition.' }
            if ($body -notmatch '[\u4e00-\u9fff]') { throw 'Missing Chinese content.' }
        } elseif ($article -ne 'cpp-modules-build-optimization.md') {
            if (-not $body.Contains('## 中文') -or -not $body.Contains('## English')) { throw 'Missing bilingual section.' }
        }
        Write-Output "$article : $($body.Length) characters; content and image references verified"
    }
} finally { $readback.Dispose() }
Write-Output $zipPath
