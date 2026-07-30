param([string]$ConfigPath, [string]$PackDir)

$ErrorActionPreference = "Continue"

$raw = Get-Content $ConfigPath -Raw -Encoding UTF8
if ($raw[0] -eq [char]0xfeff) { $raw = $raw.Substring(1) }

# JSON文本中的路径以两种形式存在：
#   正斜杠：  "path": "D:/pyProj/HUAWEISDK/..."
#   反斜杠：  "path": "D:\\pyProj\\HUAWEISDK\\..."
# 原始格式(尾斜杠、大小写)完全保留

$raw = $raw.Replace("D:/pyProj/HUAWEISDK", "D:/test_runner_ui/sdk")
$raw = $raw.Replace("D:\\pyProj\\HUAWEISDK", "D:\\test_runner_ui\\sdk")

# 添加 config_path 字段
$j = $raw | ConvertFrom-Json
$j | Add-Member -MemberType NoteProperty -Name "config_path" -Value "D:/.SDKtest/config.json" -Force -ErrorAction SilentlyContinue
$out = Join-Path $PackDir "config.json"
$j | ConvertTo-Json -Depth 10 | Set-Content $out -Encoding UTF8
