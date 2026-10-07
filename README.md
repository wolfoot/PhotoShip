# Pixel Studio 0.1

Qt 6 + C++ 实现的 Ubuntu / Windows 类 Photoshop 桌面编辑器。参考 `/work/misc/Compositor` 的图层模型和工作流，复用其 MIT 授权的魔棒及边界追踪 C 算法。Compositor 源码目录没有被修改。

![界面预览](docs/preview.png)

## 第一版功能

- 多文档标签、暗色桌面界面、图层缩略图、重命名、显示/隐藏、复制、删除、上下排序。
- 分组与嵌套，在 Properties → Group 中移动图层；分组透明度/可见性传递到后代。
- 12 种混合模式：Normal、Multiply、Screen、Overlay、Darken、Lighten、Color Dodge、Color Burn、Hard Light、Soft Light、Difference、Exclusion。
- 非破坏移动、缩放、旋转、翻转；Properties 可输入精确值，Move 工具右下角手柄可缩放。
- 画笔、橡皮、大小/透明度、矩形/椭圆选区、套索、连续魔棒；Shift 添加选区，Alt 减去选区。
- 图层灰度蒙版：白色蒙版、选区生成、绘制、禁用、反相、移除。勾选 Edit mask 后黑色隐藏、白色显示。
- 可编辑文字与矩形/椭圆形状；需要像素编辑时在 Layer 菜单栅格化。
- 色阶、RGB 曲线（输入控制点）、HSV 色相/饱和度/明度、反相，作用于当前栅格层并遵守选区。
- 裁剪、画布尺寸、撤销/重做、剪贴板图像、拖放导入、缩放与平移。
- PNG/JPEG 导入和导出。额外图片格式取决于所安装 Qt 插件，首版只保证 PNG/JPEG。
- 可编辑 `.psproj` 文件夹项目；安全保存、重新打开、脏状态及关闭前保存提示。
- 基础 `.comp` 项目导入：栅格图层、分组、支持的混合模式、变换和关联栅格蒙版。不支持的特性明确报错，不会静默丢弃；导入后另存 `.psproj`，不覆盖原项目。

## 本机启动

```bash
./scripts/run.sh
./scripts/run.sh --demo
./scripts/run.sh /absolute/path/to/project.psproj
```

当前工作区有仅在 `.deps/` 中解压的 Qt 6.2.4 开发库和运行库，启动脚本自动使用它们。没有修改系统安装；`.deps/` 和 `build/` 不进入源码分发。

## Ubuntu 构建与安装

基线：Ubuntu 22.04/24.04 x86_64，Qt 6.2+，CMake 3.21+，C++17。

```bash
sudo apt update
sudo apt install qt6-base-dev qt6-qpa-plugins cmake ninja-build g++
./scripts/build-linux.sh
sudo apt install ./dist/pixelstudio-0.1.0-Linux.deb
pixelstudio
```

`.deb` 动态依赖系统 Qt，不把完整 Qt 打入包内。安装时由 apt 补齐依赖；离线安装需要事先准备对应依赖包。首次测试和界面截图使用 Qt offscreen 平台，不要求显示服务器：

```bash
QT_QPA_PLATFORM=offscreen ./scripts/run.sh --screenshot /tmp/pixelstudio.png
```

## Windows 构建与安装

目标：Windows 10/11 x64。准备 Visual Studio 2022 的 Desktop development with C++ 工作负载、CMake、Qt 6.8.3 的 MSVC 2022 64-bit 组件。可选安装 Inno Setup 6。

```powershell
./scripts/build-windows.ps1 -QtPrefix 'C:\Qt\6.8.3\msvc2022_64' -Installer
```

生成：

- `dist/PixelStudio-0.1.0-windows-x64.zip`：含 Qt 运行库的便携版。
- `dist/PixelStudio-0.1.0-windows-x64-setup.exe`：Inno Setup 安装器，默认安装到当前用户目录。

省略 `-Installer` 只生成便携 ZIP。`.github/workflows/build.yml` 配置了 Ubuntu/Windows 构建、测试和包产物上传，尚未在远程运行。当前 Linux 环境没有完成 Windows 编译或实机验证。

## 快捷键

| 操作 | 快捷键 |
|---|---|
| 移动 / 画笔 / 橡皮 | V / B / E |
| 矩形 / 椭圆选区 / 套索 / 魔棒 | M / Shift+M / L / W |
| 裁剪 / 矩形形状 / 椭圆形状 | C / U / Shift+U |
| 文字 / 吸管 / 手型 | T / I / H |
| 平移 / 缩放 | 空格拖动或中键 / 滚轮 |
| 缩小 / 放大画笔 | [ / ] |
| 撤销 / 重做 | Ctrl+Z / Ctrl+Y 或 Ctrl+Shift+Z |
| 新建 / 打开 / 保存 / 另存 | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| 图片导入 / 导出 | Ctrl+Shift+O / Ctrl+Shift+E |
| 复制图层 / 新建图层 | Ctrl+J / Ctrl+Shift+N |
| 全选 / 取消选区 | Ctrl+A / Ctrl+D |
| 适应画布 / 100% | Ctrl+0 / Ctrl+1 |
| 取消当前笔画或拖动 | Esc |

## 文件格式与数据保护

`.psproj` 是含 `manifest.json` 和 `images/` 的文件夹。格式标识为 `org.pixelstudio.project`，版本 1，8-bit sRGB；JSON 的图层顺序为从底到顶。

保存时写入新的唯一 PNG 资产，然后用 `QSaveFile` 原子提交清单。清单提交成功后才清理旧资产，失败会保留旧清单及旧图层。`QLockFile` 防止同时保存同一目录。崩溃发生在提交之前时可能留下未引用的 PNG，但已保存内容仍有效。这不是多进程实时协作格式。

加载在临时 State 中验证 UUID、尺寸、像素预算、文件路径、父子关系、类型及资产，全部成功后才替换文档。禁止资产路径逃逸及符号链接。选区、撤销历史、缩放位置不写入项目。文字依赖本机字体，不嵌入字体，所以不同机器可能有字体替代或排版差异。

## 当前边界

- 首版采用 CPU/QPainter 合成，尚未实现 GPU、分块渲染或后台保存；大图操作可能阻塞界面。
- 每边最多 8192 像素，画布最多 1600 万像素，全部图层源像素最多 3200 万，蒙版最多另计 3200 万，最多 256 图层。限制会主动拒绝超量输入。
- 撤销最多 100 步，按唯一像素缓冲区统计约 256 MiB 的历史加当前图层存储预算。达到预算会裁掉旧历史；活跃图层本身超过预算时不能靠裁掉历史降低其占用。界面合成缓存、临时滤镜、缩略图等另占内存，这不是整个进程的内存硬上限。
- 撤销用 Qt 隐式共享快照，修改某层会复制该层像素一次；尚未实现笔画图块差量。
- 调整是可撤销的像素操作，暂不提供非破坏调整图层；曲线采用分段线性插值。蒙版与图层关联，不支持独立蒙版变换、分组蒙版或剪贴蒙版。
- 形状是填充矩形/椭圆；文字采用 Qt 文本排版。没有钢笔路径、富文本或高级图层样式。
- 尚未支持 PSD/PSB、RAW、16-bit/CMYK、ICC 工作空间切换、AI 抠图、液化、修复、图层合并、选区像素搬移或自动恢复。
- 混合模式使用 Qt 的合成公式，不承诺与 Photoshop/Compositor 每个像素完全一致。

## 验证

`tests/editor_tests.cpp` 是可执行的核心及界面回归检查，使用运行时检查而不是 Release 下失效的 assert。覆盖图层、分组、蒙版、变换绘画、选区、魔棒、调色、撤销、保存/加载、非法输入、PNG/JPEG 导出和鼠标选区事件。

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

本机使用 `.deps` 时由 `scripts/build-linux.sh` 自动设置 Qt 路径。本机 Release 回归检查 76 项通过；AddressSanitizer / UndefinedBehaviorSanitizer 检查通过（未进行泄漏检查）。另外已通过独立 Xvfb 虚拟显示上的 X11 窗口启动及截图检查，并验证从 DEB 解压的可执行文件能够启动。

这不代表已验证真实桌面、数位板或不同显卡驱动。可在安装 Xvfb 后运行 `python3 scripts/check-x11.py` 重做 X11 冒烟检查。

## 许可证

应用代码为 MIT。Compositor 原始版权声明保留在 `third_party/compositor/LICENSE`。Qt 动态库及插件采用各自许可证，详见 `THIRD_PARTY_NOTICES.md`。发行时保留对应 Qt 及插件许可通知。
