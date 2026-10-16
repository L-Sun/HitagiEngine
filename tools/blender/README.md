# Blender–Hitagi

通用静态网格 USD 工具。保留选中网格导出、原生 USD 导入、贴图打包和几何校验；角色准备、材质分类、控制图生成及 viewer 通信已移除。

## 安装和使用

需要 Blender 4.5 或更高版本。扩展实现、清单和许可证直接位于 `tools/blender/`，测试就近放在 `tools/blender/test/`。

```powershell
blender --command extension build --source-dir tools/blender --output-dir temp/blender-hitagi
```

在 Blender Preferences 的 Install from Disk 中安装生成的 ZIP。3D View 侧栏提供 Import Hitagi USD、Validate Selection 和 Export Hitagi Asset。材质面板仅提供底图覆盖和 alpha cutoff。

## 导出范围

- 评估当前网格和修改器，三角化并按材质拆分；不修改原始对象。
- 世界变换烘焙为右手系、Y-up、米；处理负缩放绕序和法线变换。
- UV 和法线按 face-varying 保存，保留 UV 接缝。
- 材质输出标准 UsdPreviewSurface：常量底色或直接连接的底色图片、常量 opacity、opacityThreshold；roughness 固定为 0.8。
- 图片保存为内容哈希命名的 PNG，通过相对路径引用；USD 与 textures 目录可以一起搬迁。
- 输出同名 `.hitagi.json` 清单，记录源文件、纹理哈希和网格统计；校验或导出阶段失败时保留原有 USD。

仅支持 Principled BSDF 常量底色、直接 Image Texture 或明确的底图覆盖。复杂节点、映射坐标、UDIM 和超出 [0,1] 的贴图 UV 必须先烘焙。贴图 alpha 不导出为 opacity 连接。动画、骨骼、实例、灯光、相机和任意材质图往返不在范围内。

导入使用 Blender 原生 USD importer，不恢复旧角色专用元数据。它不是完整材质往返编辑系统。

## 验证

```powershell
blender --background --factory-startup --python-exit-code 1 --python tools/blender/test/pipeline_test.py
```

Blender 集成测试覆盖变换、负缩放、UV 接缝、法线、多材质、修改器、贴图搬迁与导入、导出失败保留旧文件，以及 Operator 和注册/卸载。
