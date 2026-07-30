param([string]$ConfigPath, [string]$PackDir)

$ErrorActionPreference = "Continue"

$j = Get-Content $ConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
$old = @("D:/pyProj/HUAWEISDK", "D:\\pyProj\\HUAWEISDK")
$new = @("D:/test_runner_ui/sdk", "D:\\test_runner_ui\\sdk")

foreach ($p in $j.profiles) {
    for ($i = 0; $i -lt 2; $i++) {
        if ($p.test_binary) { $p.test_binary = $p.test_binary.Replace($old[$i], $new[$i]) }
        $nd = @(); foreach ($d in $p.dependencies) { $nd += $d.Replace($old[$i], $new[$i]) }
        $p.dependencies = $nd
        if ($p.env_vars.MODEL_DIR) { $p.env_vars.MODEL_DIR = $p.env_vars.MODEL_DIR.Replace($old[$i], $new[$i]) }
    }
}
$j.config_path = "D:/.SDKtest/config.json"
$out = Join-Path $PackDir "config.json"
$j | ConvertTo-Json -Depth 10 | Set-Content $out -Encoding UTF8