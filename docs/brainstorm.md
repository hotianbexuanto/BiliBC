# BiliBC - Bilibili风格弹幕视频播放器

## 头脑风暴文档

> 日期: 2026-03-04
> 状态: 初始头脑风暴

---

## 1. 项目概述

开发一个本地视频播放器，具备 Bilibili 风格的滚动弹幕功能。
界面追求现代化设计：毛玻璃效果 + Material Design 3 风格。
使用 C/C++ 实现，追求高性能和低内存占用。

**核心特点：**
- 本地视频 + 网络视频播放
- 支持三种弹幕格式：.ass / .json / .xml（Bilibili格式）
- 弹幕可选加载（默认无弹幕）
- 现代化UI + 毛玻璃效果
- GitHub Actions 构建（不依赖本地构建环境）

---

## 2. 技术选型（已确认）

```
┌──────────────────────────────────────────────────┐
│                  BiliBC 播放器                      │
├──────────────────────────────────────────────────┤
│  UI层     │ SDL2 窗口 + OpenGL 自定义UI渲染         │
│  模糊效果  │ OpenGL 着色器（高斯模糊 / Kawase模糊）  │
│  弹幕渲染  │ OpenGL + FreeType（GPU加速文字渲染）    │
│  视频引擎  │ libmpv（嵌入式 mpv 播放器库）           │
│  网络支持  │ libmpv 内置（HTTP/HLS/DASH）           │
│  编译器    │ MSVC (Visual Studio)                   │
│  构建系统  │ CMake + GitHub Actions                  │
└──────────────────────────────────────────────────┘
```

### 2.1 为什么选 libmpv？

libmpv 是 mpv 播放器的 C 库版本，一行代码就能播放任何格式的视频：

```c
mpv_handle *mpv = mpv_create();
mpv_initialize(mpv);
const char *cmd[] = {"loadfile", "video.mp4", NULL};
mpv_command(mpv, cmd);
```

**它自动帮你处理：**
- ✅ 所有视频格式（MP4, MKV, FLV, AVI, WebM, MOV...）
- ✅ 所有音频格式（AAC, MP3, FLAC, Opus...）
- ✅ 音视频同步（A/V sync）
- ✅ 硬件加速解码（DXVA2, D3D11VA）
- ✅ 网络流播放（HTTP, HTTPS, HLS, DASH）
- ✅ 字幕渲染（ASS/SRT，内置 libass）
- ✅ 快进快退（seek）
- ✅ 变速播放
- ✅ 音轨/字幕轨切换

**你不需要自己实现这些复杂功能。**

### 2.2 为什么选 SDL2？

SDL2 是一个成熟的多媒体库，负责：
- 创建窗口
- 处理键盘/鼠标输入
- 提供 OpenGL 上下文
- 跨平台（虽然目前目标是 Windows）

### 2.3 为什么选 OpenGL + FreeType？

- OpenGL 配合 SDL2 使用很自然
- FreeType 是最成熟的字体渲染库，完美支持中文
- GPU加速的弹幕渲染可以轻松处理数百条同时显示的弹幕
- 着色器可以实现毛玻璃模糊效果

---

## 3. 弹幕系统设计

### 3.1 三种弹幕文件格式

**格式对比：**

| 格式 | 来源 | 时间精度 | 包含信息 | 解析难度 |
|------|------|----------|----------|----------|
| `.xml` | Bilibili原始格式 | 秒（小数） | 时间/模式/字号/颜色/内容 | 简单（XML解析） |
| `.json` | Bilibili API格式 | 毫秒 | 时间/模式/字号/颜色/权重/用户hash/内容 | 简单（JSON解析） |
| `.ass` | 字幕格式（转换后） | 百分秒 | 时间/样式/移动动画/颜色/内容 | 中等（需解析ASS语法） |

**XML格式（最核心）** - 参数结构 `<d p="时间,模式,字号,颜色,发送时间,保留,用户hash,弹幕ID">内容</d>`
```xml
<d p="3.561,1,25,16777215,1772547939,0,6d2d7b1f,2059040366199584000">可爱可爱！！</d>
```

**JSON格式** - 结构化字段
```json
{
  "progress": 3561,      // 毫秒时间戳
  "mode": 1,             // 1=滚动, 4=底部, 5=顶部
  "fontsize": 25,        // 字号
  "color": 16777215,     // 10进制RGB颜色
  "content": "可爱可爱！！"  // 弹幕文字
}
```

**ASS格式** - 带动画的字幕事件
```
Dialogue: 0,0:00:03.56,0:00:09.56,Medium,,0,0,0,,{\move(2076,30,-156,30,0,6000)\c&HFFFFFF&}可爱可爱！！
```

### 3.2 弹幕模式（mode）

| mode值 | 类型 | 说明 |
|--------|------|------|
| 1 | 滚动弹幕 | 从右向左滚动（最常见） |
| 4 | 底部弹幕 | 固定在底部中央 |
| 5 | 顶部弹幕 | 固定在顶部中央 |
| 6 | 逆向弹幕 | 从左向右滚动（少见） |
| 7 | 高级弹幕 | 可设定路径和动画（复杂，可暂不实现） |

### 3.3 弹幕渲染流程

```
加载弹幕文件 → 统一解析为内部结构体 → 按时间排序
                                          ↓
视频播放中 → 获取当前时间 → 查找该时刻的弹幕 → 分配轨道（防重叠）→ GPU渲染
```

### 3.4 内部弹幕数据结构（C语言）

```c
typedef struct {
    double time;          // 出现时间（秒）
    int mode;             // 1=滚动, 4=底部, 5=顶部
    int fontsize;         // 字号
    uint32_t color;       // RGB颜色
    char content[512];    // 弹幕文字（UTF-8）
    // --- 运行时数据 ---
    float x, y;           // 当前位置
    float speed;          // 滚动速度
    int track;            // 分配的轨道
    bool active;          // 是否正在显示
} Danmaku;
```

### 3.5 轨道分配算法（防弹幕重叠）

屏幕分为多条水平轨道，每条弹幕分配到一条空闲轨道：

```
轨道0: [可爱可爱！！ →→→]
轨道1: [来了! →→→→→→→]
轨道2: (空)  ← 下一条弹幕分配到这里
轨道3: [又是你们俩吗？ →→]
...
```

---

## 4. UI/界面设计

### 4.1 整体布局（类Bilibili风格）

```
┌─────────────────────────────────────────────────────┐
│                                                       │
│                    视频画面                             │
│         [弹幕在这里从右向左滚动飘过]                     │
│                                                       │
│                                                       │
├─────────────────────────────────────────────────────┤
│  ▶ advancement                                       │
│  ═══════════●═══════════════════════ 01:23 / 04:56   │
│                                                       │
│  ▶ ⏮ ⏭  🔊━━━━  ⟨弹幕⟩  ⛶全屏                       │
│         [毛玻璃效果控制栏]                               │
└─────────────────────────────────────────────────────┘
```

### 4.2 控制栏功能

```
左侧:  [播放/暂停] [上一个] [下一个] [音量滑块]
中间:  [进度条 + 时间显示]
右侧:  [弹幕开关] [弹幕设置] [倍速] [全屏]
```

### 4.3 毛玻璃效果实现

使用 **Kawase Blur** 着色器（比高斯模糊快5-10倍，效果相似）：

```
步骤:
1. 将视频画面渲染到 FBO (帧缓冲对象)
2. 对控制栏区域进行多次降采样模糊
3. 叠加半透明深色蒙版
4. 在上面绘制UI控件
```

效果类似 Windows 11 的 Acrylic / iOS 的毛玻璃。

### 4.4 配色方案（Material Design 3 风格）

```
主色调:    #1E1E2E (深色背景)
强调色:    #89B4FA (蓝色高亮)
文字:      #CDD6F4 (浅色文字)
次要文字:  #6C7086 (灰色)
控制栏:    rgba(30, 30, 46, 0.7) + 模糊
进度条:    #89B4FA (已播放) / #45475A (未播放)
弹幕文字:  白色 + 黑色描边(1-2px)
```

---

## 5. 完整功能清单

### 5.1 第一阶段 - 核心播放（MVP）

- [ ] SDL2 窗口创建 + OpenGL 上下文
- [ ] libmpv 嵌入 + 基础视频播放
- [ ] 播放/暂停（空格键）
- [ ] 进度条 + 拖动seek
- [ ] 音量控制（上下方向键 / 鼠标滚轮）
- [ ] 全屏切换（F键 / 双击）
- [ ] 打开本地文件（文件对话框）
- [ ] 基础控制栏UI（半透明背景，先不做模糊）

### 5.2 第二阶段 - 弹幕系统

- [ ] XML弹幕文件解析
- [ ] JSON弹幕文件解析
- [ ] ASS弹幕文件解析
- [ ] FreeType字体加载 + 中文渲染
- [ ] 滚动弹幕（mode=1）
- [ ] 顶部/底部弹幕（mode=4,5）
- [ ] 弹幕颜色支持
- [ ] 弹幕轨道分配（防重叠）
- [ ] 弹幕开关按钮
- [ ] 弹幕透明度调节
- [ ] 弹幕字号调节
- [ ] 弹幕速度调节

### 5.3 第三阶段 - 毛玻璃UI

- [ ] Kawase Blur着色器
- [ ] 控制栏毛玻璃效果
- [ ] 弹幕设置面板（毛玻璃背景）
- [ ] 鼠标悬停效果
- [ ] 圆角按钮/滑块
- [ ] 过渡动画（控制栏淡入淡出）

### 5.4 第四阶段 - 网络 + 完善

- [ ] 网络URL输入框
- [ ] HTTP/HTTPS视频播放
- [ ] HLS/DASH流媒体支持
- [ ] 拖放文件打开
- [ ] 快捷键完善（左右=快进退5秒，上下=音量）
- [ ] 倍速播放（0.5x / 1x / 1.5x / 2x）
- [ ] 记住窗口位置和大小
- [ ] 记住上次播放进度

---

## 6. 依赖库清单

| 库 | 用途 | 许可证 | 大小 |
|----|------|--------|------|
| **libmpv** | 视频/音频播放引擎 | GPL-2.0+ / LGPL-2.1+ | ~15MB |
| **SDL2** | 窗口管理、输入处理、OpenGL上下文 | zlib | ~2MB |
| **FreeType** | 字体渲染（弹幕文字） | FreeType License / GPL-2.0 | ~1MB |
| **cJSON** | JSON弹幕文件解析 | MIT | ~50KB |
| **libxml2** 或 **mxml** | XML弹幕文件解析 | MIT | ~1MB / ~200KB |
| **stb_image** | 可选，加载图标/图片 | Public Domain | ~100KB (header-only) |

**预估总大小：** ~20MB（可分发包）
**预估内存占用：** ~40-80MB（取决于视频分辨率）

---

## 7. GitHub Actions CI/CD 构建

### 7.1 构建流程

```yaml
# .github/workflows/build.yml
name: Build BiliBC
on: [push, pull_request]

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4

      # 安装依赖
      - name: Install vcpkg dependencies
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgJsonGlob: 'vcpkg.json'

      # 下载 libmpv
      - name: Download libmpv dev package
        run: |
          curl -L -o mpv-dev.7z https://github.com/shinchiro/mpv-winbuild-cmake/releases/download/latest/mpv-dev-x86_64.7z
          7z x mpv-dev.7z -ompv-dev

      # CMake 构建
      - name: Configure & Build
        run: |
          cmake -B build -G "Visual Studio 17 2022" -A x64
          cmake --build build --config Release

      # 打包发布
      - name: Package Release
        run: |
          mkdir release
          cp build/Release/BiliBC.exe release/
          cp mpv-dev/libmpv-2.dll release/
          cp mpv-dev/mpv-2.dll release/
          # 复制其他依赖DLL...

      - uses: actions/upload-artifact@v4
        with:
          name: BiliBC-Windows-x64
          path: release/
```

### 7.2 依赖管理

使用 **vcpkg** 管理大部分依赖（SDL2, FreeType, cJSON, libxml2）：

```json
// vcpkg.json
{
  "name": "bilibc",
  "version": "0.1.0",
  "dependencies": [
    "sdl2",
    "freetype",
    "cjson",
    "libxml2"
  ]
}
```

libmpv 通过直接下载预编译包获取（mpv官方提供Windows预编译版本）。

---

## 8. 项目目录结构

```
BiliBC/
├── .github/
│   └── workflows/
│       └── build.yml          # GitHub Actions 构建配置
├── src/
│   ├── main.c                 # 程序入口
│   ├── player.c/.h            # libmpv 播放器封装
│   ├── danmaku.c/.h           # 弹幕系统（加载、管理、更新）
│   ├── danmaku_parser.c/.h    # 弹幕文件解析（xml/json/ass）
│   ├── renderer.c/.h          # OpenGL 渲染器（弹幕绘制、UI绘制）
│   ├── text.c/.h              # FreeType 文字渲染
│   ├── ui.c/.h                # UI控件（按钮、滑块、进度条）
│   ├── blur.c/.h              # 毛玻璃模糊着色器
│   ├── window.c/.h            # SDL2 窗口管理
│   └── utils.c/.h             # 工具函数
├── shaders/
│   ├── blur.vert              # 模糊顶点着色器
│   ├── blur.frag              # 模糊片段着色器
│   ├── text.vert              # 文字顶点着色器
│   ├── text.frag              # 文字片段着色器
│   └── ui.frag                # UI渲染着色器
├── assets/
│   └── fonts/                 # 默认字体文件
├── docs/
│   └── brainstorm.md          # 本文档
├── CMakeLists.txt             # CMake 构建脚本
├── vcpkg.json                 # vcpkg 依赖清单
└── README.md
```

---

## 9. 初学者注意事项

由于你是 C/C++ 初学者，以下是一些重要建议：

### 9.1 难度评估

| 模块 | 难度 | 说明 |
|------|------|------|
| SDL2窗口+输入 | ⭐⭐ | SDL2 API 简单清晰，教程多 |
| libmpv集成 | ⭐⭐ | API简单，但需要理解回调机制 |
| 弹幕XML/JSON解析 | ⭐⭐ | 使用cJSON和libxml2，直接调API |
| FreeType文字渲染 | ⭐⭐⭐⭐ | 需要理解字形、纹理图集、UTF-8编码 |
| OpenGL弹幕渲染 | ⭐⭐⭐ | 需要学习OpenGL基础（VAO/VBO/着色器） |
| 毛玻璃着色器 | ⭐⭐⭐⭐ | 需要理解FBO、多pass渲染、着色器语言 |
| 轨道分配算法 | ⭐⭐ | 纯逻辑，不难 |
| UI控件系统 | ⭐⭐⭐ | 按钮/滑块需要处理鼠标交互状态 |

### 9.2 推荐学习顺序

1. **先学 SDL2** → 创建窗口、处理事件、显示一个颜色
2. **学 OpenGL基础** → 画三角形、纹理、着色器
3. **集成 libmpv** → 让视频在窗口中播放
4. **学 FreeType** → 在屏幕上渲染中文文字
5. **实现弹幕解析** → 读取XML/JSON文件
6. **实现弹幕滚动** → 把文字画在视频上滚动
7. **最后做UI和模糊** → 这是最花时间的部分

### 9.3 可降级方案

如果某些部分太难，可以暂时降级：

- **FreeType太难？** → 先用 `stb_truetype.h`（单头文件，更简单）
- **OpenGL着色器太难？** → 先用固定管线或SDL2_ttf渲染文字
- **毛玻璃效果太难？** → 先用简单半透明背景，后面再加
- **ASS格式太复杂？** → 先只支持XML和JSON，ASS后面再加

---

## 10. 已确认的额外需求

| 需求 | 决定 |
|------|------|
| 播放列表 | ❌ 暂不需要 |
| 弹幕发送 | ❌ 暂不需要 |
| 截图功能 | ✅ 需要（快捷键截取当前画面，可选含/不含弹幕） |
| 界面语言 | ✅ 中英双语（可切换） |
| 最低Windows版本 | ✅ Windows 10（可使用现代API） |
| 跨平台 | ✅ 代码结构预留跨平台抽象层，以后可移植到 Linux/Mac |

### 10.1 截图功能设计

```
快捷键: Ctrl+S 或 PrintScreen
- 截图包含弹幕: 直接读取当前OpenGL帧缓冲
- 截图不含弹幕: 只读取视频帧（通过libmpv的screenshot命令）
- 保存格式: PNG
- 保存位置: 视频同目录 或 用户图片文件夹
```

### 10.2 中英双语实现

```c
// 简单的i18n方案 - 字符串表
typedef struct {
    const char *play;
    const char *pause;
    const char *open_file;
    const char *danmaku_on;
    const char *danmaku_off;
    // ...
} LangStrings;

static const LangStrings lang_zh = {
    .play = "播放",
    .pause = "暂停",
    .open_file = "打开文件",
    .danmaku_on = "弹幕开",
    .danmaku_off = "弹幕关",
};

static const LangStrings lang_en = {
    .play = "Play",
    .pause = "Pause",
    .open_file = "Open File",
    .danmaku_on = "Danmaku ON",
    .danmaku_off = "Danmaku OFF",
};
```

### 10.3 跨平台抽象层

代码结构保持平台无关，平台相关代码隔离：

```
src/
├── platform/
│   ├── platform.h          # 平台抽象接口
│   ├── platform_win32.c    # Windows实现（当前）
│   ├── platform_linux.c    # Linux实现（未来）
│   └── platform_macos.c    # macOS实现（未来）
```

隔离的平台相关功能：
- 文件对话框（Win32: `GetOpenFileName`, Linux: zenity/kdialog）
- 截图保存路径获取
- 系统字体路径
- 高DPI缩放

---

## 总结

这个项目的技术栈是：

```
SDL2（窗口）→ OpenGL（渲染）→ libmpv（视频）→ FreeType（文字）→ 自定义弹幕系统
```

对于初学者来说，最大的挑战在于 **OpenGL + FreeType** 的文字渲染部分。
建议严格按照分阶段顺序开发，先让视频能播放，再加弹幕，最后做毛玻璃UI。

预计开发时间（兼职开发）：
- 第一阶段（基础播放）：2-3周
- 第二阶段（弹幕系统）：3-4周
- 第三阶段（毛玻璃UI）：2-3周
- 第四阶段（网络+截图+双语+完善）：3-4周
- **总计：约 2.5-3.5个月**

---

## 所有决策汇总

| 决策项 | 选择 |
|--------|------|
| 架构方案 | **libmpv + 自定义UI** |
| 窗口框架 | **SDL2** |
| 渲染引擎 | **OpenGL** |
| 弹幕渲染 | **OpenGL + FreeType** |
| 毛玻璃效果 | **自定义GPU着色器（Kawase Blur）** |
| 网络支持 | **完整（HTTP/HLS/DASH，libmpv内置）** |
| 编译器 | **MSVC (Visual Studio)** |
| 构建方式 | **GitHub Actions + vcpkg** |
| 最低系统 | **Windows 10** |
| 截图 | **✅ 支持** |
| 界面语言 | **中英双语** |
| 跨平台 | **预留抽象层，以后移植** |
| 弹幕格式 | **XML + JSON + ASS 三种** |
