# BiliBC 实施计划

> 基于 brainstorm.md 的决策，结合技术研究结果
> 日期: 2026-03-04
> 状态: 实施计划

---

## 技术栈确认

| 组件 | 技术 | 说明 |
|------|------|------|
| 窗口/输入 | **SDL2** | 创建窗口、OpenGL上下文、键鼠事件 |
| OpenGL加载 | **GLAD** (GL 3.3 Core) | 生成后提交到仓库，不用vcpkg |
| 视频引擎 | **libmpv** (render API) | 渲染到FBO纹理，我们再合成弹幕 |
| 文字渲染 | **FreeType** + 动态字形图集 | GPU加速中文弹幕渲染，带描边 |
| UI模糊 | **Kawase Blur着色器** | 降采样+上采样双向模糊，性能极高 |
| JSON解析 | **cJSON** | 解析Bilibili JSON弹幕 |
| XML解析 | **yxml** (内嵌) | 超轻量XML流解析器，~2KB，不用vcpkg |
| 构建系统 | **CMake** + **vcpkg** + **MSVC** | GitHub Actions自动构建 |

---

## 阶段一：项目骨架 + 基础视频播放

### 步骤 1.1：创建项目结构和构建系统

**创建文件：**

```
BiliBC/
├── .github/workflows/build.yml
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── vcpkg-configuration.json
├── app.manifest
├── third_party/
│   ├── glad/
│   │   ├── include/glad/glad.h
│   │   ├── include/KHR/khrplatform.h
│   │   └── src/glad.c
│   └── yxml/
│       ├── yxml.h
│       └── yxml.c
├── src/
│   └── main.c
└── shaders/
    ├── quad.vert
    └── quad.frag
```

**vcpkg.json:**
```json
{
  "name": "bilibc",
  "version-string": "0.1.0",
  "dependencies": ["sdl2", "freetype", "cjson"]
}
```

> 注意：不用libxml2，改用内嵌的yxml（~2KB，零依赖）

**CMakeLists.txt 关键配置：**
```cmake
project(BiliBC VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 17)

find_package(SDL2 CONFIG REQUIRED)
find_package(Freetype CONFIG REQUIRED)
find_package(cJSON CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# GLAD (内嵌)
add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# yxml (内嵌)
add_library(yxml STATIC third_party/yxml/yxml.c)
target_include_directories(yxml PUBLIC third_party/yxml)

# libmpv (预编译下载)
# 通过 -DMPV_DIR=<path> 传入
add_library(libmpv SHARED IMPORTED)
# ... (在CI中由lib.exe从.def生成.lib)

add_executable(BiliBC WIN32 src/main.c ...)
target_link_libraries(BiliBC PRIVATE
    SDL2::SDL2 SDL2::SDL2main
    Freetype::Freetype cjson::cjson
    OpenGL::GL glad yxml libmpv
)
```

**GitHub Actions 构建流程：**
1. `actions/checkout@v4`
2. `ilammy/msvc-dev-cmd@v1` — 设置MSVC环境
3. `lukka/run-vcpkg@v11` — 安装vcpkg依赖（自动缓存）
4. 下载 shinchiro/mpv-winbuild-cmake 最新 mpv-dev 包
5. `lib /def:mpv.def /out:mpv.lib /machine:x64` — 生成MSVC导入库
6. `cmake -B build -G Ninja` + `cmake --build build`
7. 打包exe+所有dll → `actions/upload-artifact@v4`
8. 打tag时自动创建GitHub Release

**app.manifest（Windows清单）：**
- UTF-8 活动代码页（Win10 1903+）
- Per-Monitor V2 DPI感知
- 启用视觉样式

### 步骤 1.2：SDL2窗口 + OpenGL上下文 + GLAD

```c
// src/main.c — 程序入口

#include <glad/glad.h>
#include <SDL2/SDL.h>

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    // 请求 OpenGL 3.3 Core
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window *window = SDL_CreateWindow("BiliBC",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

    SDL_GLContext gl = SDL_GL_CreateContext(window);
    gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
    SDL_GL_SetSwapInterval(1); // VSync

    // 主循环
    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }
        glClearColor(0.118f, 0.118f, 0.180f, 1.0f); // #1E1E2E
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

### 步骤 1.3：集成 libmpv (渲染到FBO)

**核心架构：**
```
mpv 渲染视频帧 → 我们的 FBO 纹理
      ↓
我们绘制全屏四边形（视频纹理）→ 屏幕
      ↓
在上面绘制弹幕/UI → 屏幕
```

**创建 src/player.h / player.c：**

```c
// src/player.h
typedef struct {
    mpv_handle         *mpv;
    mpv_render_context *mpv_gl;
    GLuint              video_fbo;
    GLuint              video_texture;
    int                 fbo_w, fbo_h;

    // 状态
    double time_pos;    // 当前播放时间（秒）
    double duration;    // 总时长
    double volume;      // 音量 0-100
    int    paused;      // 是否暂停
    int    file_loaded; // 是否已加载文件
} Player;

int  player_init(Player *p);
void player_create_render_context(Player *p);  // 需要GL上下文
void player_load_file(Player *p, const char *path);
void player_toggle_pause(Player *p);
void player_seek(Player *p, double seconds); // 相对跳转
void player_set_volume(Player *p, double vol);
void player_render_frame(Player *p, int width, int height); // 渲染到FBO
GLuint player_get_texture(Player *p);  // 获取视频纹理
double player_get_time(Player *p);      // 获取当前时间
void player_handle_events(Player *p);   // 处理mpv事件
void player_destroy(Player *p);
```

**关键实现要点：**

1. `mpv_set_option_string(mpv, "vo", "libmpv")` — 必须在initialize前设置
2. `mpv_set_option_string(mpv, "hwdec", "auto")` — 启用硬件解码
3. 创建FBO + 纹理，让mpv渲染到FBO
4. 使用 `mpv_render_context_set_update_callback` 通知新帧就绪
5. 回调在mpv线程中调用 → 只能设flag + `SDL_PushEvent`
6. 主循环检测flag → 调用 `mpv_render_context_render`
7. 用 `mpv_observe_property` 观察 time-pos, pause, duration, volume

### 步骤 1.4：全屏四边形着色器（显示视频纹理）

**shaders/quad.vert:**
```glsl
#version 330 core
const vec2 pos[4] = vec2[4](
    vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, 1)
);
const vec2 uv[4] = vec2[4](
    vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(1, 1)
);
out vec2 v_uv;
void main() {
    gl_Position = vec4(pos[gl_VertexID], 0, 1);
    v_uv = uv[gl_VertexID];
}
```

**shaders/quad.frag:**
```glsl
#version 330 core
in vec2 v_uv;
out vec4 frag_color;
uniform sampler2D u_texture;
void main() {
    frag_color = texture(u_texture, v_uv);
}
```

### 步骤 1.5：基础快捷键

| 快捷键 | 功能 |
|--------|------|
| 空格 | 播放/暂停 |
| ← → | 快退/快进 5秒 |
| ↑ ↓ | 音量 +5/-5 |
| F / F11 | 全屏 |
| Q / Esc | 退出 |
| 鼠标滚轮 | 音量调节 |
| 双击 | 全屏切换 |
| 拖放文件 | 打开视频 |

### 步骤 1.6：文件打开对话框

```c
// src/platform/platform.h
// 平台抽象接口
char *platform_open_file_dialog(const char *title, const char *filters);
char *platform_save_file_dialog(const char *title, const char *filters);
const char *platform_get_font_path(void);  // 系统默认中文字体路径

// src/platform/platform_win32.c
#include <windows.h>
#include <commdlg.h>

char *platform_open_file_dialog(const char *title, const char *filters) {
    OPENFILENAMEW ofn = {0};
    wchar_t file[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"视频文件\0*.mp4;*.mkv;*.avi;*.flv;*.webm;*.mov\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        // 转换 wchar_t → UTF-8 返回
    }
    return NULL;
}
```

**阶段一完成标准：**
- [ ] 窗口打开，显示深色背景
- [ ] 能打开本地视频文件播放
- [ ] 播放/暂停/快进退/音量工作
- [ ] 全屏切换工作
- [ ] 拖放文件打开
- [ ] GitHub Actions 构建成功并产出artifact

---

## 阶段二：弹幕系统

### 步骤 2.1：弹幕数据结构和管理器

**创建 src/danmaku.h / danmaku.c：**

```c
// src/danmaku.h
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    double   time;           // 出现时间（秒）
    int      mode;           // 1=滚动, 4=底部, 5=顶部, 6=逆向
    int      fontsize;       // 原始字号
    uint32_t color;          // RGB颜色 (0xRRGGBB)
    char     content[512];   // UTF-8文字
    // --- 运行时 ---
    float    x, y;           // 当前屏幕位置
    float    speed;          // 滚动速度 (px/s)
    int      track;          // 分配的轨道号
    float    text_width;     // 渲染后的文字宽度(px)
    bool     active;         // 是否正在显示
    double   start_time;     // 开始显示的时间
    double   end_time;       // 结束显示的时间
} Danmaku;

typedef struct {
    Danmaku *items;          // 动态数组，按time排序
    int      count;          // 总弹幕数
    int      capacity;

    // 活跃弹幕（当前在屏幕上的）
    Danmaku **active;        // 指针数组
    int       active_count;
    int       active_capacity;

    // 轨道管理
    double  *track_end_time; // 每条轨道的"最后一条弹幕离开"时间
    int      num_tracks;     // 轨道数量

    // 搜索优化
    int      next_index;     // 下一条待激活的弹幕索引

    // 设置
    float    opacity;        // 弹幕透明度 0-1
    float    font_scale;     // 字号缩放
    float    speed_scale;    // 速度缩放
    bool     enabled;        // 弹幕开关
    float    display_area;   // 显示区域比例 (0.5 = 上半屏)
} DanmakuManager;

void danmaku_manager_init(DanmakuManager *dm);
void danmaku_manager_load(DanmakuManager *dm, const char *filepath);
void danmaku_manager_clear(DanmakuManager *dm);
void danmaku_manager_update(DanmakuManager *dm, double time, double dt,
                            int screen_w, int screen_h);
void danmaku_manager_seek(DanmakuManager *dm, double time);
void danmaku_manager_destroy(DanmakuManager *dm);
```

### 步骤 2.2：弹幕文件解析器

**创建 src/danmaku_parser.h / danmaku_parser.c：**

三种格式统一解析到 `Danmaku` 数组：

```c
// 自动检测文件类型（按扩展名）
int danmaku_load_file(const char *path, Danmaku **out, int *out_count);

// 分别解析
int danmaku_parse_xml(const char *data, size_t len, Danmaku **out, int *out_count);
int danmaku_parse_json(const char *data, size_t len, Danmaku **out, int *out_count);
int danmaku_parse_ass(const char *data, size_t len, Danmaku **out, int *out_count);
```

**XML解析（用yxml流式解析器）：**
```c
// 解析 <d p="3.561,1,25,16777215,1772547939,0,6d2d7b1f,ID">内容</d>
// p 属性的逗号分隔值: 时间,模式,字号,颜色,发送时间,保留,用户hash,ID
```

**JSON解析（用cJSON）：**
```c
// 解析数组中每个对象的 progress(ms), mode, fontsize, color, content
```

**ASS解析（自己写简单解析器）：**
```c
// 解析 Dialogue 行
// 提取 \move(x1,y1,x2,y2,t1,t2) 获取运动信息
// 提取 \c&HBBGGRR& 获取颜色（注意BGR顺序！）
// 提取纯文字内容（去掉所有 {...} 标签）
// 时间格式: H:MM:SS.cc (百分秒)
```

### 步骤 2.3：FreeType + 动态字形图集

**创建 src/text.h / text.c：**

```c
// src/text.h
typedef struct {
    FT_Library  library;
    FT_Face     face;
    FT_Stroker  stroker;        // 用于描边
    int         font_size_px;
    int         stroke_width_px;
} FontContext;

typedef struct {
    uint32_t codepoint;
    // 图集中的位置
    uint16_t atlas_x, atlas_y, atlas_w, atlas_h;
    // 描边版本的位置
    uint16_t stroke_x, stroke_y, stroke_w, stroke_h;
    // 字形度量
    int16_t  bearing_x, bearing_y;
    int16_t  stroke_bearing_x, stroke_bearing_y;
    int16_t  advance_x;
    // LRU
    uint64_t last_frame;
} CachedGlyph;

typedef struct {
    GLuint      texture;          // GL_RG8 纹理（R=描边, G=填充）
    int         atlas_size;       // 2048
    // 字形缓存 (哈希表)
    CachedGlyph glyphs[4096];
    int         glyph_count;
    int         hash_table[8192]; // 开放寻址哈希表
    // 货架式打包器
    // ...
    FontContext *font;
    uint64_t    current_frame;
} GlyphAtlas;

void font_context_init(FontContext *ctx, const char *font_path,
                       int size_px, int stroke_px);
void glyph_atlas_init(GlyphAtlas *atlas, FontContext *font);
CachedGlyph *glyph_atlas_get(GlyphAtlas *atlas, uint32_t codepoint);
void glyph_atlas_prewarm(GlyphAtlas *atlas); // 预加载常用汉字
float text_measure_width(GlyphAtlas *atlas, const char *utf8_text);
```

**关键设计决策：**
- 使用 `GL_RG8` 双通道纹理：R通道=描边alpha，G通道=填充alpha
- 这样一次draw call就能同时渲染描边和填充
- 用 `FT_Stroker` 生成矢量描边（比偏移8次画好得多）
- 预热加载 ASCII + 最常用500个汉字，避免冷启动卡顿
- 每帧限制最多16次新字形光栅化，避免弹幕暴增时卡顿

**弹幕文字着色器（单pass描边+填充）：**
```glsl
// shaders/danmaku.frag
#version 330 core
in vec2 v_uv;
out vec4 frag_color;
uniform sampler2D u_atlas;
uniform vec3 u_fill_color;    // 弹幕文字颜色
uniform vec3 u_stroke_color;  // 描边颜色（通常黑色）
uniform float u_opacity;      // 弹幕透明度

void main() {
    vec2 sample = texture(u_atlas, v_uv).rg;
    float stroke_a = sample.r;
    float fill_a   = sample.g;
    vec3 color = mix(u_stroke_color, u_fill_color, fill_a);
    float alpha = max(stroke_a, fill_a) * u_opacity;
    frag_color = vec4(color * alpha, alpha); // 预乘alpha
}
```

### 步骤 2.4：批量文字渲染器

**创建 src/text_renderer.h / text_renderer.c：**

```c
// 批量渲染所有弹幕文字
// 每个字形 = 6个顶点（2个三角形）
// 500条弹幕 × 10字 = 30,000顶点 → GPU轻松处理

typedef struct { float x, y, u, v; } TextVertex;

typedef struct {
    GLuint program;
    GLuint vao, vbo;
    TextVertex vertices[65536]; // 最大顶点数
    int vertex_count;
    GlyphAtlas *atlas;
} TextRenderer;

void text_renderer_init(TextRenderer *r, GlyphAtlas *atlas);
void text_renderer_add(TextRenderer *r, const char *text,
                       float x, float y);
void text_renderer_flush(TextRenderer *r, float *projection,
                         float fill_r, float fill_g, float fill_b,
                         float stroke_r, float stroke_g, float stroke_b,
                         float opacity);
```

### 步骤 2.5：轨道分配算法

```c
// 核心思路：
// 屏幕被分成 N 条水平轨道（轨道高度 = 字体高度 + 间距）
// 新弹幕到来时，找一条"安全"的轨道分配
//
// "安全"条件（滚动弹幕）:
// 1. 该轨道上一条弹幕已经完全进入屏幕（不会追尾）
// 2. 新弹幕不会在到达屏幕左边前追上前面那条
//
// 如果所有轨道都满了 → 随机选一条（或跳过该弹幕）

int danmaku_assign_track(DanmakuManager *dm, Danmaku *d,
                         int screen_w, int screen_h) {
    float track_height = d->fontsize * dm->font_scale + 4; // +4间距
    int max_tracks = (int)(screen_h * dm->display_area / track_height);

    for (int t = 0; t < max_tracks; t++) {
        if (track_is_available(dm, t, d, screen_w)) {
            return t;
        }
    }
    return -1; // 所有轨道满了
}
```

### 步骤 2.6：弹幕渲染主循环

```c
// 每帧执行：
void danmaku_render(DanmakuManager *dm, TextRenderer *tr,
                    double video_time, double dt,
                    int screen_w, int screen_h) {
    if (!dm->enabled) return;

    // 1. 激活新弹幕（根据当前视频时间）
    danmaku_activate_new(dm, video_time, screen_w, screen_h);

    // 2. 更新活跃弹幕位置
    for (int i = 0; i < dm->active_count; i++) {
        Danmaku *d = dm->active[i];
        if (d->mode == 1) { // 滚动
            d->x -= d->speed * dt;
        }
        // mode 4,5 固定位置不需要更新
    }

    // 3. 移除过期弹幕
    danmaku_remove_expired(dm, video_time);

    // 4. 批量渲染
    float projection[16];
    make_ortho(projection, screen_w, screen_h);

    for (int i = 0; i < dm->active_count; i++) {
        Danmaku *d = dm->active[i];
        uint8_t r = (d->color >> 16) & 0xFF;
        uint8_t g = (d->color >> 8)  & 0xFF;
        uint8_t b =  d->color        & 0xFF;

        text_renderer_add(tr, d->content, d->x, d->y);
    }

    // 统一flush（1次draw call画完所有弹幕）
    text_renderer_flush(tr, projection,
                        1.0f, 1.0f, 1.0f,    // 填充色（每条弹幕可不同时需分批）
                        0.0f, 0.0f, 0.0f,    // 描边色
                        dm->opacity);
}
```

> **注意：** 如果弹幕颜色各不相同，需要：
> - 方案A：按颜色分组batch，每组一次draw call
> - 方案B：把颜色作为顶点属性传入（推荐，一次draw call）

**阶段二完成标准：**
- [ ] 能加载 .xml 弹幕文件
- [ ] 能加载 .json 弹幕文件
- [ ] 能加载 .ass 弹幕文件
- [ ] 弹幕从右向左滚动，不重叠
- [ ] 弹幕有黑色描边，中文渲染清晰
- [ ] 弹幕颜色正确
- [ ] 弹幕和视频时间同步
- [ ] seek后弹幕正确重置
- [ ] 弹幕开关按钮工作

---

## 阶段三：毛玻璃UI

### 步骤 3.1：Kawase双向模糊着色器

**创建 src/blur.h / blur.c：**

使用 Dual Kawase Blur（KDE/picom/Hyprland 都用这个算法）：

```
视频帧 → 降采样(4次) → 上采样(4次) → 模糊纹理
                                         ↓
                              合成到控制栏区域
```

**着色器文件：**

```glsl
// shaders/blur_down.frag — 降采样（5次采样，权重4:1:1:1:1）
#version 330 core
uniform sampler2D u_texture;
uniform vec2 u_halfpixel;
uniform float u_offset;
in vec2 v_texcoord;
out vec4 frag_color;
void main() {
    vec2 uv = v_texcoord;
    vec4 sum = texture(u_texture, uv) * 4.0;
    sum += texture(u_texture, uv + vec2(-u_halfpixel.x, +u_halfpixel.y) * u_offset);
    sum += texture(u_texture, uv + vec2(+u_halfpixel.x, +u_halfpixel.y) * u_offset);
    sum += texture(u_texture, uv + vec2(+u_halfpixel.x, -u_halfpixel.y) * u_offset);
    sum += texture(u_texture, uv + vec2(-u_halfpixel.x, -u_halfpixel.y) * u_offset);
    frag_color = sum / 8.0;
}
```

```glsl
// shaders/blur_up.frag — 上采样（8次采样）
#version 330 core
uniform sampler2D u_texture;
uniform vec2 u_halfpixel;
uniform float u_offset;
in vec2 v_texcoord;
out vec4 frag_color;
void main() {
    vec2 uv = v_texcoord;
    vec2 hp = u_halfpixel * u_offset;
    vec4 sum  = texture(u_texture, uv + vec2(-hp.x * 2.0, 0.0));
    sum      += texture(u_texture, uv + vec2(+hp.x * 2.0, 0.0));
    sum      += texture(u_texture, uv + vec2(0.0, +hp.y * 2.0));
    sum      += texture(u_texture, uv + vec2(0.0, -hp.y * 2.0));
    sum      += texture(u_texture, uv + vec2(-hp.x, +hp.y)) * 2.0;
    sum      += texture(u_texture, uv + vec2(+hp.x, +hp.y)) * 2.0;
    sum      += texture(u_texture, uv + vec2(+hp.x, -hp.y)) * 2.0;
    sum      += texture(u_texture, uv + vec2(-hp.x, -hp.y)) * 2.0;
    frag_color = sum / 12.0;
}
```

**性能：** 1080p 4次迭代仅需 ~0.1-0.3ms GPU时间（比高斯模糊快33倍）

**FBO链：** 每次降采样分辨率减半
```
960×540 → 480×270 → 240×135 → 120×67 → 60×33
     ↑ 上采样回来 ↑         ↑          ↑
```

### 步骤 3.2：合成着色器（毛玻璃效果）

```glsl
// shaders/composite.frag
#version 330 core
uniform sampler2D u_blurred;
uniform sampler2D u_scene;
uniform vec4 u_tint;           // 叠加颜色，如 rgba(0.12, 0.12, 0.18, 0.7)
uniform vec4 u_blur_rect;      // 模糊区域(归一化坐标) {x, y, w, h}
uniform float u_frost_opacity; // 毛玻璃不透明度

in vec2 v_texcoord;
out vec4 frag_color;

void main() {
    vec2 uv = v_texcoord;
    vec2 rect_min = u_blur_rect.xy;
    vec2 rect_max = u_blur_rect.xy + u_blur_rect.zw;

    float in_region = step(rect_min.x, uv.x) * step(uv.x, rect_max.x)
                    * step(rect_min.y, uv.y) * step(uv.y, rect_max.y);

    vec4 sharp = texture(u_scene, uv);
    vec4 blurred = texture(u_blurred, uv);

    vec4 bg = mix(sharp, blurred, in_region * u_frost_opacity);
    bg = mix(bg, bg + u_tint, in_region * u_tint.a);
    frag_color = bg;
}
```

### 步骤 3.3：UI控件系统

**创建 src/ui.h / ui.c：**

```c
// 基于 immediate-mode 的简单UI系统
// （类似 Dear ImGui 的思路，但更轻量）

typedef struct {
    float x, y, w, h;
} UIRect;

typedef struct {
    // 鼠标状态
    float mouse_x, mouse_y;
    int   mouse_down;
    int   mouse_clicked;

    // 控制栏
    float control_bar_opacity; // 淡入淡出
    float control_bar_timer;   // 鼠标不动后隐藏计时

    // 弹幕设置面板
    int   danmaku_panel_open;
} UIState;

// 控件函数（返回是否被点击/值是否改变）
int  ui_button(UIState *ui, UIRect rect, const char *label);
int  ui_slider(UIState *ui, UIRect rect, float *value, float min, float max);
int  ui_toggle(UIState *ui, UIRect rect, int *enabled, const char *label);
void ui_progress_bar(UIState *ui, UIRect rect, double current, double total,
                     double *seek_to);  // seek_to != NULL 表示用户拖拽了
```

**控制栏行为：**
- 鼠标移动/按下 → 显示控制栏（opacity = 1.0）
- 鼠标静止 3秒 → 淡出控制栏（opacity → 0.0，0.3秒过渡）
- 全屏时鼠标到底部 → 显示控制栏
- 控制栏区域 = 屏幕底部 ~15% 区域

### 步骤 3.4：圆角矩形渲染

```glsl
// shaders/rounded_rect.frag
// 用SDF（有符号距离场）画圆角矩形
uniform vec4 u_rect;      // {x, y, w, h} 屏幕像素坐标
uniform float u_radius;   // 圆角半径
uniform vec4 u_color;     // 填充颜色

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) - r;
}
```

**阶段三完成标准：**
- [ ] 控制栏有毛玻璃模糊背景
- [ ] 控制栏鼠标空闲后自动隐藏（淡出动画）
- [ ] 播放/暂停按钮可点击
- [ ] 进度条可拖拽seek
- [ ] 音量滑块可拖拽
- [ ] 弹幕开关按钮
- [ ] 弹幕设置面板（透明度/字号/速度滑块）
- [ ] 圆角UI元素

---

## 阶段四：网络 + 截图 + 双语 + 完善

### 步骤 4.1：网络视频播放

libmpv 内置完整网络支持，直接传URL即可：

```c
player_load_file(&player, "https://example.com/video.mp4");
player_load_file(&player, "https://example.com/stream.m3u8"); // HLS
```

需要实现的UI：
- URL输入框（文字输入控件）
- 粘贴URL（Ctrl+V支持）
- 加载中状态指示

### 步骤 4.2：截图功能

```c
// 方案A：含弹幕截图 → 读取当前OpenGL帧缓冲
void screenshot_with_danmaku(int screen_w, int screen_h, const char *save_path) {
    GLubyte *pixels = malloc(screen_w * screen_h * 4);
    glReadPixels(0, 0, screen_w, screen_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    // 上下翻转（OpenGL原点在左下）
    // 用 stb_image_write 保存为PNG
    stbi_write_png(save_path, screen_w, screen_h, 4, flipped_pixels, stride);
    free(pixels);
}

// 方案B：不含弹幕截图 → 让mpv截图
void screenshot_video_only(Player *p) {
    const char *cmd[] = {"screenshot-to-file", save_path, "video", NULL};
    mpv_command(p->mpv, cmd);
}
```

快捷键：`Ctrl+S`（含弹幕）/ `Ctrl+Shift+S`（不含弹幕）

### 步骤 4.3：中英双语

```c
// src/i18n.h
typedef struct {
    const char *play;
    const char *pause;
    const char *open_file;
    const char *open_url;
    const char *danmaku_on;
    const char *danmaku_off;
    const char *danmaku_settings;
    const char *danmaku_opacity;
    const char *danmaku_size;
    const char *danmaku_speed;
    const char *volume;
    const char *fullscreen;
    const char *screenshot;
    const char *speed;
    const char *load_danmaku;
    // ...
} LangStrings;

extern const LangStrings LANG_ZH;
extern const LangStrings LANG_EN;
extern const LangStrings *current_lang;

#define _(key) (current_lang->key)
// 使用: ui_button(ui, rect, _("play"));
```

### 步骤 4.4：倍速播放

```c
// 通过 mpv 的 speed 属性
void player_set_speed(Player *p, double speed) {
    mpv_set_property(p->mpv, "speed", MPV_FORMAT_DOUBLE, &speed);
}

// UI: 倍速选择菜单
// 0.5x | 0.75x | 1.0x | 1.25x | 1.5x | 2.0x
```

### 步骤 4.5：记住设置

```c
// 保存到 JSON 配置文件
// %APPDATA%/BiliBC/config.json
{
    "window_x": 100,
    "window_y": 100,
    "window_w": 1280,
    "window_h": 720,
    "volume": 80,
    "danmaku_opacity": 0.8,
    "danmaku_font_scale": 1.0,
    "danmaku_speed_scale": 1.0,
    "danmaku_enabled": true,
    "language": "zh",
    "last_file": "...",
    "last_position": 123.45
}
```

**阶段四完成标准：**
- [ ] 网络URL输入 → 播放
- [ ] 截图功能（含/不含弹幕）
- [ ] 中英双语切换
- [ ] 倍速播放菜单
- [ ] 窗口位置/大小记忆
- [ ] 上次播放进度记忆
- [ ] 所有设置持久化

---

## 渲染流水线总览

每一帧的渲染顺序：

```
┌─────────────────────────────────────────────────┐
│ 1. mpv渲染视频帧 → video_fbo (纹理)               │
│                                                   │
│ 2. Kawase模糊 → blurred_texture                   │
│    (视频纹理降采样4次再上采样4次)                     │
│                                                   │
│ 3. 合成到屏幕：                                    │
│    a. 画视频纹理全屏                               │
│    b. 控制栏区域用模糊纹理替代(毛玻璃效果)           │
│    c. 画弹幕文字(预乘alpha混合)                    │
│    d. 画UI控件(按钮/滑块/进度条)                   │
│                                                   │
│ 4. SDL_GL_SwapWindow                             │
└─────────────────────────────────────────────────┘
```

---

## 源文件清单

| 文件 | 行数估计 | 难度 | 说明 |
|------|----------|------|------|
| `src/main.c` | ~300 | ⭐⭐ | 程序入口、主循环 |
| `src/player.c/h` | ~400 | ⭐⭐⭐ | libmpv封装 |
| `src/danmaku.c/h` | ~350 | ⭐⭐ | 弹幕管理器、轨道分配 |
| `src/danmaku_parser.c/h` | ~400 | ⭐⭐ | XML/JSON/ASS解析 |
| `src/text.c/h` | ~500 | ⭐⭐⭐⭐ | FreeType + 字形图集 |
| `src/text_renderer.c/h` | ~250 | ⭐⭐⭐ | 批量文字渲染 |
| `src/blur.c/h` | ~300 | ⭐⭐⭐⭐ | Kawase模糊管线 |
| `src/ui.c/h` | ~500 | ⭐⭐⭐ | UI控件系统 |
| `src/shader.c/h` | ~150 | ⭐⭐ | 着色器编译/链接工具 |
| `src/i18n.c/h` | ~100 | ⭐ | 多语言字符串 |
| `src/config.c/h` | ~150 | ⭐⭐ | 设置读写 |
| `src/platform/platform_win32.c` | ~150 | ⭐⭐ | Win32文件对话框等 |
| `src/utf8.c/h` | ~80 | ⭐⭐ | UTF-8解码迭代器 |
| **着色器 (shaders/)** | ~200 | ⭐⭐⭐ | 6个GLSL文件 |
| **总计** | **~3,800行** | | |

---

## 开发时间估算

| 阶段 | 内容 | 预计时间 |
|------|------|----------|
| 阶段一 | 骨架+构建+视频播放 | 2-3 周 |
| 阶段二 | 弹幕系统 | 3-4 周 |
| 阶段三 | 毛玻璃UI | 2-3 周 |
| 阶段四 | 网络+截图+双语+完善 | 2-3 周 |
| **总计** | | **约 2.5-3.5 个月** |

---

## 实施顺序优先级

```
构建系统 → SDL2窗口 → libmpv视频 → 快捷键
    ↓
弹幕解析 → FreeType → 弹幕渲染 → 轨道分配
    ↓
模糊着色器 → 控制栏UI → 进度条/按钮
    ↓
网络URL → 截图 → 双语 → 配置持久化
```
