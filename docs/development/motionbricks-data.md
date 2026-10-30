# MotionBricks / Sour 初音数据准备

已打通真实模型推理、Blender 资源标定，以及 Hitagi 运行时控制、重定向和脚部修正。
交互入口为 `examples/motionbricks/README.md`：WASD 移动、Shift 慢跑、松键停步、R 重置。
该 demo 使用无动画曲线的 `sour_runtime.usdc`，由 TensorRT 在线生成动作。
以下保留离线对照流程供资源验证；角色→SOMA 姿态反馈尚未实现。

## 数据位置与来源

数据包位于 `assets/motionbricks/`，由现有 `.gitignore` 排除，不提交模型权重、
第三方动作和角色文件。`manifest.json` 记录来源版本、文件大小、SHA-256 和验证范围。

- MotionBricks 官方仓库：<https://github.com/NVlabs/motionbricks>
- 固定提交：`54fc563aeaaf819a753d72cc9fdc6713989cdd43`。
- FP32 / mixed FP16：`upstream/motionbricks/assets/onnx/soma/`。
- 模型张量与关节顺序：同目录 `soma_n1.json`。
- 官方 USD 参考骨架：`upstream/motionbricks/assets/human/`。
- 38 段原始 BVH：`upstream/assets/motions/soma/`。
- 8 份 SOMA Smart Primitive 和场景：`upstream/assets/smart_primitives/soma/`、
  `upstream/assets/scenes/`。
- Sour 式初音 Ver.1.02：`character/source/Black.pmx`，附 White 版本、纹理和作者说明。
  原始来源 <https://bowlroll.net/file/146103>，复用本机已有资产，不重新下载或修改网格。
- 角色源文件校验值：`04fbfe59e4ea8ef1704b32bef71516b20afbd1e4abf1880dbc4f107e225a90ea`。

## 派生数据

| 文件 | 内容 |
| --- | --- |
| `derived/clip_inventory.json` | 每段动作的帧数、实际采样率、通道顺序、层级、米制 OFFSET、模型关节到 BVH 索引 |
| `character/skeleton.json` | 319 个原始骨骼、蒙皮权重使用统计、刚体、约束；原始 MMD 坐标与单位 |
| `character/provenance.json` | 既有 Sour 资产来源记录 |
| `derived/retarget_candidates.json` | 30 个模型关节的目标骨候选及未解决项，**不是可执行重定向配置** |

角色解析数据来自已有 `temp/motion-sour/model.json`，其源 PMX 哈希与复制的原件一致。
它使用 `MMDParser.parsePmx(buffer, true)`；未在此阶段猜测米制缩放、旋转基或脚底偏移。
腿部同时存在控制骨和 D 变形骨，不能忽略 grant 关系直接写入二者。
脊柱、颈部、下颌、眼睛、手指端点的对应需要进一步标定，不能凭名称补出不存在的骨骼。

## 必须保留的数据约定

- 神经模型为 30 关节、30 Hz、Y-up、米制；6D 旋转为模型空间旋转矩阵前两列。
- 原始 BVH 为 **78 个关节，包含额外的 Root**，本次文件头采样率约
  **120.0048 Hz**（Frame Time 的小数精度所致）。不能当作 30 Hz 原样送入模型。
- BVH 平移为厘米；官方读取器转换为米。位置通道存在时使用通道值，不能再重复加 OFFSET。
- 每关节 Euler 通道顺序必须按文件解释。模型 30 关节索引不是 BVH 原始索引，
  也不是角色蒙皮 palette 索引；需要先处理完整层级，再提取模型空间特征。
- BVH 第 0 帧是动作帧，不能当成角色绑定姿态；OFFSET 也不能替代 USD 中完整的参考变换。
- 读取器、USD 单位转换和接触定义的上游源文件已随数据保留，供移植时核对。
- 4 帧历史 + 最多 4 帧目标；输出 `[1,64,366]` 和 token 数。
  有效 raw 帧为 `4 * token_count`，前后各去掉 3 帧后用于播放。
- 4 个接触值依次为左脚跟、左脚尖、右脚跟、右脚尖。模型预测经运行时夹到 `[0,1]`；
  原始 BVH 不自带这些接触标签，不能把推断标签称为 ground truth。

## 验证状态与下一步

准备脚本已核对所有选定 LFS 文件的官方 SHA-256 和长度；解析检查全部 38 段、
47,887 帧的通道数、帧数、有限数值和 30 个模型关节覆盖。
两份 ONNX 是完整权重文件，不是 Git LFS 指针。
FP32 ONNX 已通过 TensorRT 11.4 引擎编译和真实输入推理；本次输出 16 tokens，
解码为 58 帧有效动画。mixed FP16 尚未验证。
Blender 5.2.3 导出的 345 关节动画逐帧与求值姿态比较，最大位置误差约
0.0000041 米；58 帧实际鞋子蒙皮顶点最低高度约 0.000028 米，没有穿过零高度地面。
这不是任意动作、任意场景的无碰撞保证。
Hitagi 的 DX12/Toon 播放检查通过，20 个 mesh、49,572 个三角形、1 条 1.9 秒动画。

后续覆盖与运行时工作：

1. 用官方 USD 参考骨架与 BVH 读取器建立源数据播放对照；按时间重采样至 30 Hz，
   对旋转插值并验证前向运动学，避免直接截取原始 Euler 数据。
2. 验证 FP32 ONNX 的真实输入输出，再比较 mixed FP16。保留固定输入和数值误差记录。
3. 对 Sour 主身体骨链进行单位、参考姿态、骨轴、控制/变形骨与脚底接触点标定。
   使用 idle、walk、jog、run、crouch、sit 的现有动作作为覆盖集；不把文件名当成接触标签。
4. 建立 SOMA→角色与角色→SOMA 的离线误差基线，之后接实时求解和反馈。

## Blender 资源流水线

脚本为 `tools/motionbricks/blender_prepare.py`，使用 Blender 自带的 numpy、pxr，
以及既有 `temp/blender_mmd_tools` 插件和已校验的 `temp/motion-sour/model.json`。
当前资源为该 Sour 模型的专用基线，不能直接当作通用角色转换器。

`prepared/sour_generated.blend` 保留可编辑角色、原始表情与接触控制器；
`prepared/sour_generated.usdc` 是 Hitagi 可直接播放的烘焙动画。
`prepared/retarget_calibration.json` 保存骨轴修正、参考变换和自动估计的脚底接触点，
仍需更多姿态覆盖与人工标定。角色导入缩放沿用已有资源的 0.08。
保留 D 骨 grant 和袖子辅助骨约束，避免重复驱动变形骨。
USD 按材质拆分网格、三角化、转换为 Y-up 米制，暂不导出引擎不支持的 blend shapes。

脚部修正使用模型接触置信度、迟滞、世界空间锚点和双骨 IK；
没有全身 pelvis 求解、环境碰撞、裙摆头发物理或角色→SOMA 闭环反馈。

在仓库根目录依次执行（`$trtexec` 指向 xmake 安装的 TensorRT 可执行文件，
其 bin 与匹配的 CUDA bin 须位于当前进程 PATH）：

```powershell
$blender = 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe'
& $blender --background --factory-startup --python-exit-code 1 --python tools/motionbricks/blender_prepare.py -- --max-frames 90
& $trtexec --onnx=assets/motionbricks/upstream/motionbricks/assets/onnx/soma/mmm_v6_soma_fp32.onnx --saveEngine=assets/motionbricks/derived/soma-fp32-trt11.plan --skipInference --builderOptimizationLevel=3
$fixtureDir = 'assets/motionbricks/prepared/inference_input'
$fixture = Get-Content "$fixtureDir/fixture.json" -Raw | ConvertFrom-Json
$inputs = ($fixture.input_shapes.PSObject.Properties.Name | ForEach-Object { "${_}:$fixtureDir/$_.bin" }) -join ','
& $trtexec --loadEngine=assets/motionbricks/derived/soma-fp32-trt11.plan "--loadInputs=$inputs" --warmUp=0 --duration=0 --iterations=1 --exportOutput=temp/motionbricks/inference-output.json
& $blender --background --factory-startup --python-exit-code 1 --python tools/motionbricks/blender_prepare.py -- --max-frames 90 --prediction temp/motionbricks/inference-output.json
& $blender --background --factory-startup --python-exit-code 1 --python tools/motionbricks/validate_prepared.py
xmake r usd-viewer-e2e DX12 --record-animation assets/motionbricks/prepared/sour_generated.usdc 116 1 toon physics-off
```

TensorRT 输入使用相对路径，避免 Windows 盘符的冒号被解释成张量名分隔符。
输入 fixture 与推理结果必须成对保留；导入预测不会重写 fixture。
引擎 plan 只用于当前软硬件组合，不作为可移植模型发布。
本地验证记录与预览在 `temp/motionbricks/`，引擎记录在
`temp/usd-viewer-recording/sour_generated/DX12-toon-physics-off/`。

## Hitagi 接入依据（现状，不是已实现功能）

`asset.base` 已有 JointLayout/Pose 与绑定/应用流程，Scene 最终将关节变换交给蒙皮。
`asset.plugins:rig` 已有 Aim、TwoBoneIK 和同基底 CopyLocalRotation。
Rig 按顺序执行、求解器无状态，当前明确不负责重定向、物理查询或 actor 移动，
也不是全身迭代求解器。接触锚点、时序状态、双向映射和全身约束仍需设计。
不要把神经模型的根轨迹和 Rig 控制目标分别写成互相独立的世界位置来源。

参考现有 `docs/assets/animation.md` 和 `docs/assets/rig.md`。
当前 demo 在 `examples/motionbricks/runtime.cppm` 实现专用控制器，并通过 Viewer 的
场景更新回调在 Transform/Skinning 更新之前写入骨骼。它尚未提升为通用引擎重定向组件。

## 连续步态相位

前向 Walk/Jog/Run 和 4 下的六种风格使用完整参考周期作为条件库。完成 `prepare_runtime.py` 和
`prepare_presets.py` 后，运行：

```powershell
& $blender --background --python-exit-code 1 --python tools/motionbricks/prepare_gait_cycles.py
```

输出 `prepared/gait_cycles.txt` 和 `gait_cycles.json`，记录周期长度、平均根速度和
接缝误差及左脚相位原点。每个周期保留八个额外连续样本，供跨周期末尾的四帧目标及分数帧插值使用。

控制器在起步、走跑切换时根据历史脚/膝位置和相对速度估计相位；持续移动时按动作
时间推进，并将每次规划的姿态反馈校正限制在 ±0.25 帧。匹配在角色朝向坐标系内进行，
转向保留相位。停止后清除跟踪状态，再次起步从实际历史姿态重新获取。目标相位与
位置弹簧共用一秒前瞻。走跑速度每动作帧按加速 4 m/s²、减速 5 m/s² 推进，过渡期间
每三动作帧重规划；用速度混合相邻 Walk/Jog/Run 库，将左脚相位原点对齐并使用共同周期
重采样，局部姿态与根位移一起混合。稳定状态使用该参考周期的平均根速度。
六种风格独立跟踪各自周期，保留受伤风格固有的左右差异。方向混合和环境交互沿用各自目标选择逻辑。

交互模式的 WASD 根据相机水平朝向转换到世界方向；地面投影退化时保留最近的有效方向。
`--test-camera-movement` 覆盖相机旋转后的 W/S/D，固定世界方向的旧回归继续使用原模拟输入。
`validate_speed_transition.py` 检查速度斜率、松开 Shift 后目标回收幅度和相机输入映射。

验证入口：`--test-phase` 覆盖走/跑/走、停止、重启、转向与 Jog 切换；
`validate_smart_locomotion.py` 检查真实推理输入、相位推进和校正上限。
`--side-walk --gait-probe phasefix-long --compare-out <目录>` 记录 30 秒稳态步态，
随后运行 `python tools/motionbricks/validate_gait.py <目录>` 检查源骨架和初音的左右摆动。
`--gait-probe` 是诊断入口，其他旧对照名称保留原条件供回归比较。

## 许可与重建

代码 Apache-2.0；ONNX 权重 NVIDIA Open Model Agreement；38 段动作单独使用
NVIDIA Sample Data Evaluation License。Sour 作者说明禁止商业使用，并规定修改/再分发条件。
原文随数据保存，不将这份本地评估包视为可随产品发布的资产包。

在固定提交的上游 checkout 中获取对应 LFS 文件后运行：

```powershell
git lfs pull --include="motionbricks/assets/onnx/soma/*,motionbricks/assets/human/*,assets/motions/soma/*" --exclude=""
python tools/motionbricks/prepare_data.py --upstream <upstream-checkout>
```

第一条在上游目录执行，第二条在 Hitagi 根目录执行。角色源目录与已校验解析文件须存在；
脚本使用 Python 标准库。重新准备会更新数据包文件，不会删除额外文件或改变原始资产。
