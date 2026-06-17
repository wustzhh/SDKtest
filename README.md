# YbMesher SDK 测试运行器

基于 Qt6 的 gtest 图形化测试工具，可自由选择、运行、渲染和导出测试结果。

## 功能

- ✅ **自动发现** — 通过 `--gtest_list_tests` 加载所有用例
- ✅ **按需运行** — 树形勾选 → `--gtest_filter` 单独执行指定用例
- ✅ **模型渲染** — 结构化结果树（拓扑统计、特征识别、计时等）可搜索/高亮/筛选
- ✅ **结果导出** — XLSX 报告（概要 + 统计 + 失败详情）
- ✅ **通用适配** — JSON 配置化，换套测试改路径即可

## 构建

### 前置条件

- **Qt 6.x**（Widgets 模块）
- **CMake ≥ 3.16**
- **MSVC 2019+** 或 **MinGW**

### 编译

```bash
cd ybaf-mesher/test_runner_ui

# 生成 VS 工程
cmake -B build -G "Visual Studio 17 2022"

# 编译 Release
cmake --build build --config Release
```

编译产物：

```
build/Release/test_runner_ui.exe
build/Release/config/test_config.json
```

### 配置

编辑 `config/test_config.json`：

```json
{
  "test_binary": "build/Release/ybaf-mesher-test.exe",
  "extra_args": ["--gtest_also_run_disabled_tests"],
  "categories": [
    { "name": "网格测试", "prefixes": ["meshhelper", "MeshTest"] },
    { "name": "几何测试", "prefixes": ["FixtureTest", "ut_geometry"] }
  ]
}
```

`prefixes` 匹配 Suite 名前缀，用于用例树分类。

## 使用流程

```
1. 启动 → 文件 → 编辑配置 → 填入测试 exe 路径
2. 文件 → 加载用例 → 自动发现所有 TEST/TEST_F
3. 勾选想跑的用例 → 点"▶ 运行选中"
4. 实时查看进度和日志
5. 运行完成 → 结果模型树中浏览/搜索/高亮
6. 点"📊 导出报告" → 生成 .xlsx
```

## 适配其他测试

换一套 gtest 测试，只需改 `test_config.json`：

| 字段 | 说明 |
|------|------|
| `test_binary` | 测试 exe 路径 |
| `extra_args` | 额外参数（如 `--gtest_also_run_disabled_tests`） |
| `categories[x].prefixes` | Suite 名前缀，自动分类 |
