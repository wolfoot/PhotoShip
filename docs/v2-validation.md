# Pixel Studio 0.2.0 验证记录

验证日期：2026-10-07；本机 Linux x86_64，Qt 6.2.4，Release / Debug + ASan/UBSan。

## 已通过

- Release：首版 76 项 + 第二版 126 项，共 202 项检查；`ctest --test-dir build --output-on-failure` 两个测试目标通过。
- ASan/UBSan：同样两个测试目标通过；`ASAN_OPTIONS=detect_leaks=0`，未做泄漏检查。覆盖退出、关闭标签、异步保存和恢复期间的对象生命周期。
- 第二版检查包含：图块缓存重用及局部失效、缩放接缝、笔画/新建蒙版的差量撤销取消、混合结构历史、多选分组拖动模型/复制/删除/合并、旋转图层选区搬移及扩容、调整/剪贴保存加载、取样复制和色调修复、合成 Qt 数位板压感事件。
- PSD 合成样例检查包含：raw/RLE/ZIP/ZIP 预测、RGB/alpha/栅格蒙版、Unicode、分组顺序、降级报告、截断输入、拒绝 16-bit/CMYK、合并预览、界面兼容性确认。
- 保存中继续编辑保持脏状态；保存中请求关闭不会丢弃后来编辑；干净保存后关闭文档；自动恢复写入最新快照；清理排在正在写入的恢复任务后执行。
- 子进程写入并提交恢复快照后使用 `std::_Exit(99)` 异常退出，文件保留；随后启动窗口并通过恢复对话框恢复为未保存文档。验证的是已完成快照的恢复，不是保证任意崩溃时刻都无编辑损失。
- 独立 Xvfb/X11 启动及截图通过，最终预览见 `docs/preview.png`。
- Ubuntu DEB 生成，通过解包后启动及截图验证，包内版本为 0.2.0；未修改主机系统安装。

## 外部 PSD 样例

另外从 [psd-tools 官方测试目录](https://github.com/psd-tools/psd-tools/tree/main/tests/psd_files) 下载四个文件至 `/tmp`，未收入源码包：

| 文件 | 本机导入结果 |
|---|---|
| `2layers.psd` | 101×55，2 个栅格层，Unicode 图层名和顺序正确 |
| `group.psd` | 100×200，背景、分组、组内形状缓存像素，共 3 层/组 |
| `layer_mask_data.psd` | 200×200，5 层；读取栅格蒙版，报告矢量/蒙版参数降级 |
| `clipping-mask.psd` | 360×200，7 层/组；保留嵌套分组，报告未支持的矢量等参数 |

通过独立的临时命令行检查程序解析并输出合成 PNG。此项确认可以读取这些真实文件，不代表导入后每个像素与 Photoshop 完全一致。PSD 结构实现参考 [Adobe 官方文件格式规范](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/)。

## 尚未验证

- Windows 编译、Windows 安装包及实机启动，本次按要求排除；仅维护已有构建脚本的版本与 zlib 依赖。
- 真实数位板、驱动、压感手感及真实桌面上的全部交互；压感自动测试采用 Qt 合成事件。
- 未运行本次修改后的 GitHub Actions，也未推送本次改动。
- 完整 PSD/PSB 兼容、PSD 导出、16-bit/CMYK/RAW、分组蒙版、独立蒙版变换及高级修复均不在本版范围内。具体降级和合并限制见 README。

## 交付文件

- `dist/pixelstudio-0.2.0-Linux.deb`：Ubuntu 安装包，动态依赖系统 Qt Widgets/Gui/Core/Concurrent、Qt 平台插件及 zlib。
- `dist/PixelStudio-0.2.0-source.tar.gz`：可构建源码、文档、许可和脚本；不包含 `.git`、本机依赖、构建目录、账户配置或安装包。
