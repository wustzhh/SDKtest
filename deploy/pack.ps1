param(
    [string]$ConfigPath,
    [string]$PackDir
)

$ErrorActionPreference = "Stop"

Write-Host "[2/5] Parsing config and collecting files..."

$raw = Get-Content $ConfigPath -Raw -Encoding UTF8
# remove BOM if present
if ($raw[0] -eq 0xfeff) { $raw = $raw.Substring(1) }
$config = $raw | ConvertFrom-Json
$todo = @()

foreach ($p in $config.profiles) {
    # collect test binary and its local DLLs
    $exe = $p.test_binary
    if ($exe -and (Test-Path $exe)) {
        $todo += @{src=$exe; dst='bin\' + (Split-Path $exe -Leaf)}
        $exeDir = Split-Path $exe -Parent
        Get-ChildItem $exeDir -Filter *.dll -ErrorAction SilentlyContinue | ForEach-Object {
            $todo += @{src=$_.FullName; dst='bin\' + $_.Name}
        }
    }
    # collect dependency dir DLLs
    foreach ($dep in $p.dependencies) {
        if (Test-Path $dep) {
            Get-ChildItem $dep -File -ErrorAction SilentlyContinue | ForEach-Object {
                $todo += @{src=$_.FullName; dst='bin\' + $_.Name}
            }
        }
    }
    # collect model files from MODEL_DIR
    $modelDir = $p.env_vars.MODEL_DIR
    if ($modelDir -and (Test-Path $modelDir)) {
        Get-ChildItem $modelDir -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
            $rel = $_.FullName.Substring($modelDir.Length).TrimStart('\')
            $todo += @{src=$_.FullName; dst='models\' + $rel}
        }
    }
}

# dedup
$todo = $todo | Sort-Object -Property dst -Unique

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
$config | ConvertTo-Json -Depth 10 | ForEach-Object { $_ -replace '^\xEF\xBB\xBF', '' } | Set-Content (Join-Path $PackDir 'config.json') -Encoding UTF8
Write-Host "  OK"
