param(
    [string]$ConfigPath,
    [string]$PackDir
)

$ErrorActionPreference = "Continue"

Write-Host "[2/5] Parsing config and collecting files..."

$raw = Get-Content $ConfigPath -Raw -Encoding UTF8
if ($raw[0] -eq 0xfeff) { $raw = $raw.Substring(1) }
$config = $raw | ConvertFrom-Json
$todo = @()

foreach ($p in $config.profiles) {
    Write-Host "  Profile: $($p.name)"
    $exe = $p.test_binary
    if ($exe -and (Test-Path $exe)) {
        Write-Host "    exe OK: $exe"
        $todo += @{src=$exe; dst='bin\' + (Split-Path $exe -Leaf)}
        $exeDir = Split-Path $exe -Parent
        foreach ($f in (Get-ChildItem $exeDir -Filter *.dll -ErrorAction SilentlyContinue)) {
            $todo += @{src=$f.FullName; dst='bin\' + $f.Name}
        }
        Write-Host "    dlls from exe dir collected"
    }
    foreach ($dep in $p.dependencies) {
        if (Test-Path $dep) {
            foreach ($f in (Get-ChildItem $dep -File -ErrorAction SilentlyContinue)) {
                $todo += @{src=$f.FullName; dst='bin\' + $f.Name}
            }
            Write-Host "    dep OK: $dep"
        }
    }
    $modelDir = $p.env_vars.MODEL_DIR
    if ($modelDir -and (Test-Path $modelDir)) {
        foreach ($f in (Get-ChildItem $modelDir -Recurse -File -ErrorAction SilentlyContinue)) {
            $rel = $f.FullName.Substring($modelDir.Length).TrimStart('\')
            $todo += @{src=$f.FullName; dst='models\' + $rel}
        }
        Write-Host "    models OK: $modelDir"
    }
}

# dedup
$todo = $todo | Group-Object { $_.src } | ForEach-Object { $_.Group[0] }
Write-Host "  Found $($todo.Count) files to copy"

Write-Host "[3/5] Copying files..."
$count = 0
foreach ($item in $todo) {
    $dst = Join-Path $PackDir $item.dst
    $dstDir = Split-Path $dst -Parent
    if (!(Test-Path $dstDir)) { New-Item -ItemType Directory -Path $dstDir -Force | Out-Null }
    if (Test-Path $item.src) {
        Copy-Item $item.src $dst -Force
        $count++
    }
}
Write-Host "  Copied $count files"

Write-Host "[4/5] Fixing config paths..."
foreach ($p in $config.profiles) {
    if ($p.test_binary) {
        $p.test_binary = 'D:\test_runner_ui\bin\' + (Split-Path $p.test_binary -Leaf)
    }
    $p.dependencies = @('D:\test_runner_ui\bin')
    if ($p.env_vars.MODEL_DIR) {
        $p.env_vars.MODEL_DIR = 'D:\test_runner_ui\models\'
    }
}
$config.config_path = 'D:/.SDKtest/config.json'
$config | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $PackDir 'config.json') -Encoding UTF8
Write-Host "  OK"
