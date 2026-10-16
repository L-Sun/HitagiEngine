# Blender–Hitagi

可安装的 Blender 扩展：准备角色副本，将真实选中网格导出成可搬迁的 USD 资产包，并通过本地 bridge 在 Hitagi viewer 中重新加载。插件位于 `blender_hitagi/`，不依赖仓库中的 Python 虚拟环境或 MCP SDK。

验证环境：Blender **4.5.9 LTS / Windows**；Hitagi clang-cl debug / Vulkan。最低声明版本为 Blender 4.5，其他版本与操作系统尚未实测。

## 安装与使用

构建安装包（在仓库根目录运行，`blender` 换成实际可执行文件）：

```powershell
blender --command extension build --source-dir tools/blender/blender_hitagi --output-dir temp/blender-hitagi
blender --command extension validate temp/blender-hitagi/blender_hitagi-0.1.0.zip
```

在 Blender 的 Preferences → Get Extensions → 右上菜单 → Install from Disk 中选择 ZIP，然后启用。3D View 的侧栏（N 键）新增 **Hitagi** 页签；材质属性新增 **Hitagi Anime Material** 面板。插件的安装包遵循 [Blender 扩展格式](https://docs.blender.org/manual/en/4.5/advanced/extensions/getting_started.html)。

1. 使用 **Import Hitagi USD** 导入已有资产，或直接使用自己的 Blender 网格。该入口调用 Blender 的 USD 导入器，并恢复自定义材质类别、头部参考和控制图。
2. 在材质面板将材质设为 Generic / Skin / Face / Hair / Eye。普通导出允许一个网格含多个材质；自动准备 Face/Hair/Skin 时先按材质分离网格。
   **Base Tint** 乘到底色与透明度上，可校正不同模型的明度而不改原贴图；导出再导入时保留。
3. 设置脸部中心与半宽。可指定 Head Reference 空物体，其局部 **+X 向右、+Y 向前、+Z 向上**；自动准备当前要求直立头部。
4. 选择角色网格，点击 **Prepare Anime Copy**。新集合保存独立网格与材质副本，选中状态切到副本，原始对象保留。UI 手工操作时可将原始集合隐藏以便观察。
5. 使用 **Validate Selection** 检查，然后 **Export Hitagi Asset**。修改控制图后直接重新导出，无需重新生成副本。
6. 启动 viewer，并将 Viewer Bridge 指向它的 bridge 目录。**Export and Reload in Hitagi** 先导出，随后等待 viewer 确认加载；失败时显示具体错误，不伪装成成功。

```powershell
xmake run usd-viewer-game assets/anime_viewer/miku_hitagi/miku.usda --bridge temp/usd-viewer/live
```

`Viewer Bridge` 在 Blender 中应使用绝对路径，例如 `E:/workspace/HitagiEngine/temp/usd-viewer/live`。重新加载使用现有本地 JSON mailbox，没有开启网络服务；viewer 最小化时暂停渲染，需恢复窗口。插件等待确认最多 15 秒。网络消息、资产自动上传和自动启动 viewer 均不在该按钮职责内。

## 已实现的资产准备

- 根据明确指定的材质类别处理，不通过材质名字猜测角色结构。
- Face 使用椭球法线场与原法线混合；Skin 对相近方向的邻接法线进行有限平滑。保留网格位置与拓扑，参数控制混合强度。
- Face 生成 **512²** 阴影阈值控制图，提供颊部与鼻部参数。它是可编辑的解析初稿，**不是经过美术制作的 SDF**；可在 Image Editor 中继续绘制。
- Hair 为无底色贴图的网格生成 UV atlas，并烘焙发根到发梢的方向初稿，默认 **1024²**。已有底色贴图时保留原 UV；重叠 UV 的不同发束需要手工拆分或绘制，自动生成不能保证理想方向。
- Hair 方向存储在 UV 切线空间，导出时旋转物体不会令方向留在旧世界坐标。单独缩放轴造成的非均匀变形仍应复查高光。
- 所有控制图使用 **Non-Color**，包含边缘扩展，打包为 PNG。基于 Blender [自定义法线](https://docs.blender.org/api/4.5/bpy.types.Mesh.html#bpy.types.Mesh.normals_split_custom_set)及真实 evaluated mesh 数据处理。

自动准备面向静态网格。带修改器、形态键或镜像负缩放的对象须先制作并应用静态副本；**导出器**本身支持 evaluated 修改器、当前姿态、非均匀缩放及负缩放，并修正三角形绕序。准备操作支持 Blender Undo，不修改源 USD 文件。

## 导出契约 v1

每次导出写入：

```text
character.usda
character.hitagi.json
textures/<sha256>.png
```

贴图引用相对资产目录，内容以 SHA-256 命名。纹理就绪后再原子替换 USDA；前置校验或导出失败保留旧 USDA。清单包含生成器版本、三角形数、材质分组、贴图哈希及警告。清单与 USDA 是两个文件，不构成多文件原子事务；清单的 USDA 哈希可识别中断后的不一致。旧版本留下的无引用纹理不会自动删除。

| 数据 | 约定 |
| --- | --- |
| 场景 | 右手坐标、Y-up、米；当前帧 evaluated 世界变换烘焙到顶点；场景单位参与转换 |
| 网格 | 三角化；按材质拆分 Mesh prim，避免当前 Hitagi importer 不处理材质 GeomSubset 的限制 |
| 法线 / UV | `faceVarying`，保留 UV 接缝和逐面角自定义法线；主 UV 为 `primvars:st` |
| 材质 | `UsdPreviewSurface` 底色加游戏专用输入；通用查看器可显示底色，不承诺显示 Hitagi 效果 |
| `animeSchema` | float `1`，本插件契约版本 |
| `animeMaterial` | float：0 Generic / 1 Skin / 2 Face / 3 Hair / 4 Eye |
| `animeHead` | float4：导出坐标中的头部中心 xyz、面部半宽 w |
| `animeHeadRight` / `animeHeadUp` | float4：导出坐标中的头部单位方向，w=0 |
| `animeBaseTint` | float4：带底图时的颜色与透明度因子 |
| `animeControl` | 连接到 `UsdUVTexture`，数据颜色空间 `raw` |
| Face 控制图 | R/G 为左右光照阈值，B 为头发受影遮罩；按头部平面坐标投影，UV = 0.5 + 0.5 × `(dot(p-center,right), dot(p-center,up)) / width`，超出范围 clamp |
| Hair 控制图 | RG 将 UV 切线平面方向从 [-1,1] 编码至 [0,1]，B 为发流方向与头部 up 的混合权重，A 为高光强度；使用主 UV |

只接受 Principled BSDF 的常量底色、直接 Image Texture 底色或显式 Base Image Override。复杂节点网络、映射节点、UDIM/越界重复 UV 要先烘焙，不能静默当作等价材质导出。只导出此表定义的通道；其他 Principled 参数、顶点颜色、动画、实例、灯光和相机不属于该 profile。透明度只用于 cutout。

引擎端复用 `hitagi/editor/usd.cppm` 的通用材质输入、图片和 face-varying 导入；游戏规则留在 `examples/usd_viewer/renderer.cpp` 与 `viewer.slang`。没有把角色 Shader 或 Blender 依赖加入 engine。

## 初音端到端示例

当前展示使用三目YYB的 **YYB Hatsune Miku_default_1.0ver**：

```powershell
blender --background --factory-startup --python-exit-code 1 --python tools/blender/prepare_yyb_miku_asset.py
xmake run usd-viewer-game assets/anime_viewer/yyb_miku/yyb_miku.usda --width 1816 --height 1366 --bridge temp/usd-viewer/live
```

脚本从作者 BowlRoll 配布页下载固定哈希的原包，使用固定版本 MMD Tools 的 PMX 解析器，在 Blender 中建立静态网格，再通过本插件导出。生成可编辑的 `yyb_miku.blend`、USD 包、来源清单和原作者使用说明。保留源法线、UV 和绘制好的头发纹理，生成脸部控制图；排除 MMD 专用重复 AO/假阴影层及默认收起的眼镜，最终为 **140,812 个三角形、37 个材质**。不导入骨骼动画、物理和 MMD 特效。模型限定非商业使用，不包含在插件 ZIP 或 Git 中。

以下仍可生成旧的 Animasa 对照模型：

先按 viewer 文档准备原始初音资源，再运行：

```powershell
blender --background --factory-startup --python-exit-code 1 --python tools/blender/prepare_miku_asset.py
xmake build usd-viewer-game
xmake run usd-viewer-game assets/anime_viewer/miku_hitagi/miku.usda --width 1816 --height 1366
```

生成 `assets/anime_viewer/miku_hitagi/miku.blend`（含原始与增强副本、打包贴图）、USD 包及原作者说明。模型、控制图和 `.blend` 都是本地生成资源，不包含在插件 ZIP 中。准备脚本验证 **22,961 个三角形**且顶点位置保持不变。眼睛仍使用原始贴图，没有自动重绘或声称恢复高分辨率细节。

## 验证与诊断

```powershell
blender --background --factory-startup --python-exit-code 1 --python tools/blender/test_pipeline.py
.local/usd-viewer/Scripts/python examples/usd_viewer/check_mcp.py --scene assets/anime_viewer/miku_hitagi/miku.usda --output temp/usd-viewer/blender-mcp
.local/usd-viewer/Scripts/python examples/usd_viewer/check_quality.py --scene assets/anime_viewer/miku_hitagi/miku.usda --output temp/usd-viewer/blender-quality
```

真实 Blender 集成检查覆盖：UV 接缝、法线、多个材质、世界变换与负缩放、单位、修改器、控制图原始值与搬迁后再导入、导出失败保留旧文件、原始几何不变、UI 导出 Operator 和注册/卸载。

MCP `viewer_diagnostics` 返回 `control_maps` 的实际导入路径和尺寸。`viewer_set_quality(asset_controls=False)` 可绕过控制图；`check_quality.py` 检查开关不会修改底色、法线或深度，脸部阈值只影响 Face，发流图只影响 Hair 高光。所有图来自 RenderGraph 的真实 GPU 回读，保留正脸、侧脸、侧光、身体和全身结果。

当前仍不是完整 DCC 往返编辑系统：没有骨骼动画导出、完整 MaterialX、实时同步、Blender 与 Hitagi 着色一致的 viewport shader 或模型自动重拓扑。YYB 肩部的斑驳投影由 viewer 的软阴影过滤修正，未修改皮肤贴图；其他模型仍需检查各自的阴影与材质效果。

旧的 `hitagi_toon_authoring.py` 保留为 `HitagiToonSurface` schema 验收样例，其按钮明确标记为生成示例三角形；实际资产制作请使用本扩展。
