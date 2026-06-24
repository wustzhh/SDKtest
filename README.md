# Test Runner UI — 华为 SDK gtest Qt GUI 前端

## 功能

- 加载 gtest 测试 binary，列出所有用例
- 全量运行或指定运行（`--gtest_filter`）
- 实时进度显示 + 日志
- 结果树查看 + 模型属性显示
- XLSX 报告导出
- 3D 模型查看（STEP 文件）

---

## 编译方法

### 方法一：MinGW + OpenGL（推荐）

```bash
.\build.bat
```

编译产物：`build/test_runner_ui.exe`
运行：将 `build/` 下的 exe + DLL 复制到任意目录执行。

### 方法二：MSVC + OCCT（实体 3D 渲染）

需要 vcpkg + MSVC Qt6。

#### 1. 安装 vcpkg

```bash
git clone https://github.com/microsoft/vcpkg D:\vcpkg
D:\vcpkg\bootstrap-vcpkg.bat
```

#### 2. 安装 OCCT

```bash
D:\vcpkg\vcpkg.exe install opencascade:x64-windows
```

#### 3. 编译

```batch
call "C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake -B build -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="D:/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
    -DQt6_DIR="C:/Qt/6.11.1/msvc2022_64/lib/cmake/Qt6"
cmake --build build
```

> **注意：** Qt6_DIR 路径请根据你安装的 Qt 版本调整。
> MSVC Qt6 需要用 Qt Online Installer 单独安装（选择 MSVC 2022 64-bit 组件）。

#### 4. 运行

编译完成后，将 `build/` 目录下的所有 `.dll` 文件复制到 exe 同目录，直接运行 `test_runner_ui.exe`。

### 方法三：VTK + OCCT（完整方案，参考 cae-preprocessor）

```bash
D:\vcpkg\vcpkg.exe install vtk[qt]:x64-windows opencascade:x64-windows
cmake -B build -DCMAKE_TOOLCHAIN_FILE="D:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build
```

---

## 使用方法

1. **配置测试 binary：** 编辑 `config/test_config.json`，设置 `test_binary` 为你的 gtest exe 路径
2. **加载测试：** 点击「加载测试」或 `Ctrl+L`
3. **运行测试：** 选择用例 → 点击「运行选中」或 `Ctrl+R`
4. **查看结果：** 左侧用例树勾选，右侧显示详情
5. **导出报告：** 点击「导出」或 `Ctrl+E`，生成 `.xlsx` 文件
6. **3D 模型查看：**
   - 打开 STEP 文件（右键结果树或菜单）
   - 左键拖拽旋转
   - 中键拖拽平移
   - 滚轮缩放
   - Ctrl+左键点击模型表面设置旋转锚点
   - 复位视角按钮恢复默认视角

---

## 项目结构

```
├── build.bat              MinGW 一键编译脚本
├── CMakeLists.txt         构建配置
├── config/
│   └── test_config.json   测试 binary 配置
├── src/
│   ├── main.cpp           入口
│   ├── core/              核心逻辑
│   │   ├── ConfigManager   配置管理
│   │   ├── TestLoader      用例加载
│   │   ├── TestRunner      测试执行
│   │   ├── ResultParser    结果解析
│   │   ├── XlsxWriter      Excel 导出
│   │   └── ReportExporter  报告导出
│   └── ui/                界面
│       ├── MainWindow      主窗口
│       ├── TestListPanel   用例列表
│       ├── TestProgressPanel 进度
│       ├── ModelRenderView 结果树
│       ├── ModelInfoPanel  模型信息
│       └── Model3DViewer   3D 查看器
├── models/                测试 STEP 文件
└── README.md
```
