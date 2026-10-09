"""Render the blog's original diagrams and measured chart to PNG and SVG.

Run from the repository root with matplotlib available.
"""
from pathlib import Path
import json
import statistics

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Rectangle

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "assets"
OUT.mkdir(exist_ok=True)
plt.rcParams.update({
    "font.family": "sans-serif",
    "font.sans-serif": ["Microsoft YaHei", "DejaVu Sans"],
    "font.size": 12,
    "svg.fonttype": "path",
    "axes.unicode_minus": False,
})
INK, MUTED, BLUE, TEAL, ORANGE = "#142c44", "#526b80", "#2765ad", "#087f80", "#b0642c"
BG, PALE = "#f7f9fc", "#e8f0f8"


def canvas(title, subtitle, height=7):
    fig = plt.figure(figsize=(13, height), facecolor=BG)
    ax = fig.add_axes([0.025, 0.035, 0.95, 0.82])
    ax.set(xlim=(0, 12), ylim=(0, 6))
    ax.axis("off")
    fig.text(0.04, 0.93, title, fontsize=22, fontweight="bold", color=INK)
    fig.text(0.04, 0.879, subtitle, fontsize=11, color=MUTED)
    return fig, ax


def box(ax, x, y, w, h, title, detail="", color=BLUE):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.035,rounding_size=0.1",
                              facecolor="white", edgecolor=color, linewidth=1.6))
    ax.text(x + w/2, y + h*(0.66 if detail else 0.5), title,
            ha="center", va="center", color=color, fontsize=13, weight="bold")
    if detail:
        ax.text(x + w/2, y + h*0.28, detail, ha="center", va="center", color=MUTED, fontsize=10.5)


def arrow(ax, x1, y1, x2, y2, color=MUTED):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>",
                                mutation_scale=16, linewidth=1.5, color=color,
                                shrinkA=5, shrinkB=5))


def save(fig, name):
    for ext in ("png", "svg"):
        fig.savefig(OUT / f"{name}.{ext}", dpi=180, facecolor=fig.get_facecolor())
    plt.close(fig)


fig, ax = canvas("创建边界与依赖 / Creation and dependencies",
                 "HITAGI ENGINE   ·   gfx ownership and explicit dependency injection")
box(ax, 0.3, 4.8, 3.0, 0.9, "Engine", "composition root")
box(ax, 4.4, 4.8, 3.0, 0.9, "Device", "native bootstrap / allocators")
box(ax, 8.5, 4.8, 3.1, 0.9, "GraphicsServices", "queues / bindings / compiler")
arrow(ax, 3.3, 5.25, 4.4, 5.25)
arrow(ax, 7.4, 5.25, 8.5, 5.25)
ax.text(0.4, 3.52, "创建 → / create →\n销毁 ← / destroy ←",
        ha="left", fontsize=11, color=MUTED)
box(ax, 3.5, 2.85, 5.0, 1.0, "Static Create(...) factories",
    "dispatch backend · validate ownership · inject facilities", color=TEAL)
arrow(ax, 5.9, 4.8, 5.9, 3.85)
arrow(ax, 10.0, 4.8, 8.0, 3.85)
box(ax, 0.4, 0.85, 3.2, 1.15, "Buffer / Texture", "native device · allocator · logger")
box(ax, 4.4, 0.85, 3.2, 1.15, "View / Sampler", "resource · binding facilities")
box(ax, 8.4, 0.85, 3.2, 1.15, "Command context", "native device · pool · profiling")
for x in (2, 6, 10):
    arrow(ax, 6, 2.85, x, 2.0, TEAL)
ax.text(6, 0.22, "资源不保存抽象 Device / Resources do not retain the abstract Device",
        ha="center", color=INK, fontsize=12)
save(fig, "gfx-ownership")

fig, ax = canvas("绑定偏移与记录步长 / Binding offset vs record stride",
                 "ILLUSTRATION   ·   assumed binding alignment = 256 B   ·   record size = 48 B")
ax.text(0.3, 5.45, "三个独立绑定 / Three independent bindings", fontsize=16, color=INK, weight="bold")
start, scale = 0.4, 11.1/768
for i in range(3):
    x = start + i*256*scale
    ax.add_patch(Rectangle((x, 3.65), 256*scale, 1.0, facecolor="#e7ecf2", edgecolor="white"))
    ax.add_patch(Rectangle((x, 3.65), 48*scale, 1.0, facecolor=BLUE))
    ax.text(x, 4.93, str(i*256), fontsize=12, color=BLUE)
    ax.text(x+48*scale/2, 4.15, str(i), color="white", ha="center", va="center")
    ax.text(x+155*scale, 4.15, "padding", ha="center", color=MUTED, fontsize=11)
ax.text(0.4, 3.21, "每个 descriptor 的起始位置分别满足对齐要求 / Each descriptor starts at an aligned offset",
        color=MUTED, fontsize=11)
ax.text(0.3, 2.52, "一个数组绑定 / One array binding", fontsize=16, color=INK, weight="bold")
for i in range(3):
    x = start+i*48*scale
    ax.add_patch(Rectangle((x, 1.0), 48*scale, 1.0, facecolor=TEAL, edgecolor="white"))
    ax.text(x, 2.12, str(i*48), fontsize=11, color=TEAL)
    ax.text(x+24*scale, 1.5, str(i), color="white", ha="center", va="center")
ax.text(3.2, 1.65, "record_address = view_base + index × 48", fontsize=15, color=TEAL, weight="bold")
ax.text(3.2, 1.12, "一个 descriptor，三个记录 / One descriptor, three records", fontsize=12, color=MUTED)
ax.text(0.4, 0.36, "同一比例尺 / Same scale    ·    布局仍须匹配 Shader ABI / Layout must match the shader ABI",
        fontsize=11, color=MUTED)
save(fig, "buffer-layout")

fig, ax = canvas("模块化的成本边界 / Where module build costs live",
                 "SCHEMATIC   ·   arrows indicate build prerequisites   ·   no durations implied")
ax.text(0.3, 5.64, "重复文本包含 / Repeated textual inclusion", color=INK, fontsize=15, weight="bold")
for i, name in enumerate(("resource", "command", "graph")):
    x = 0.4 + i*4
    box(ax, x, 4.05, 3.2, 1.05, f"{name} partition", "#include <heavy vendor headers>", color=ORANGE)
ax.text(6, 3.62, "每个分区都处理头文件 / Header work repeated in each partition", ha="center", color=MUTED)
ax.axhline(3.22, color="#d2dde8", linewidth=1)
box(ax, 0.4, 1.15, 2.8, 1.3, "interop.*", "vendor declarations\ncompiled at module boundaries", color=TEAL)
for y, name in ((2.4, "resource"), (1.38, "command"), (0.36, "graph")):
    box(ax, 4.4, y, 2.5, 0.75, name, color=BLUE)
    arrow(ax, 3.2, 1.8, 4.4, y+0.375, TEAL)
box(ax, 8.5, 1.15, 2.9, 1.3, "engine → editor", "downstream compilation", color=BLUE)
for y in (2.775, 1.755, 0.735):
    arrow(ax, 6.9, y, 8.5, 1.8)
ax.text(6, -0.02, "依赖链仍然限制并行 / Dependency chains still bound parallelism", ha="center", color=INK, fontsize=11)
save(fig, "module-build-graph")

data = json.loads((ROOT / "data/build-measurements.json").read_text(encoding="utf-8"))
fig, axes = plt.subplots(1, 2, figsize=(13, 6.4), facecolor=BG)
fig.subplots_adjust(left=.08, right=.96, top=.74, bottom=.22, wspace=.34)
fig.text(.04, .92, "构建时间对照 / Build time comparisons", fontsize=23, weight="bold", color=INK)
fig.text(.04, .865, "Windows x64 · Debug · clang-cl 23.1.2 · -j34 · editor + unit_tests · compiler cache off",
         color=MUTED, fontsize=11)
for ax, key, title in zip(axes, ("partitioned_xmake", "interop_completion"),
                          ("XMake 修复 / XMake fixes\n两次均值 / mean of 2 runs", "interop 补齐 / Interop completion\n单次观察 / 1 run each")):
    case = data[key]
    samples = [case["before_seconds"], case["after_seconds"]]
    values = [statistics.mean(v) for v in samples]
    ax.set_facecolor(BG)
    bars = ax.bar([0, 1], values, width=.48, color=[BLUE, TEAL], zorder=3)
    for i, (bar, val, obs) in enumerate(zip(bars, values, samples)):
        ax.text(bar.get_x()+bar.get_width()/2, val+4, f"{val:.2f} s", ha="center", color=INK, weight="bold", fontsize=14)
        if len(obs) > 1:
            ax.scatter([i-.035, i+.035], obs, s=25, c="white", edgecolors=INK, zorder=4)
    ax.set_ylim(0, 145)
    ax.set_xticks([0, 1], ["修复前 / Before", "修复后 / After"])
    ax.set_ylabel("秒 / seconds", color=MUTED)
    ax.set_title(title, color=INK, fontsize=14, pad=15)
    ax.grid(axis="y", alpha=.16, zorder=0)
    ax.spines[["top", "right", "left"]].set_visible(False)
    ax.spines["bottom"].set_color("#bac8d4")
    ax.tick_params(axis="both", length=0, labelcolor=MUTED)
    reduction = (1-values[1]/values[0])*100
    ax.text(.5, -.22, f"时间减少 / time reduced   {reduction:.2f}%", transform=ax.transAxes,
            ha="center", fontsize=13, color=TEAL, weight="bold")
fig.text(.5, .065, "不同基线，独立比较 / Different baselines; independent comparisons", ha="center", color=MUTED, fontsize=12)
save(fig, "build-results")
fig, ax = canvas("从追问到下一轮实验 / Questions that changed the investigation",
                 "HITAGI ENGINE   ·   investigation sequence, not a continuous speedup curve", height=8)
stages = [
    (0.35, 3.55, "1  分区太多了吗", "Too many partitions?", "保留提交，合并后对照\nCommit, merge, compare", BLUE),
    (4.4, 3.55, "2  为什么编译四遍", "Why four copies?", "追踪 XMake 的 BMI 复用\nInspect reuse decisions", BLUE),
    (8.45, 3.55, "3  为什么仍差 34 秒", "Why is the gap still 34 s?", "修复后重新检查解释\nRevisit the hypothesis", ORANGE),
    (0.35, 0.55, "4  整个 build 在等谁", "What is the build waiting for?", "trace + 独立对照实验\nTrace, then time without tracing", TEAL),
    (4.4, 0.55, "5  依赖能否共享入口", "Can dependencies share an entry?", "interop + 调用点语义验证\nBridge APIs, preserve semantics", TEAL),
    (8.45, 0.55, "6  这个配置还需要吗", "Do we still need this setting?", "核对遗留配置，补测试和 PR\nAudit settings, test, upstream", TEAL),
]
for x, y, zh, en, detail, color in stages:
    ax.add_patch(FancyBboxPatch((x, y), 3.2, 1.85,
                 boxstyle="round,pad=0.035,rounding_size=0.1",
                 facecolor="white", edgecolor=color, linewidth=1.6))
    ax.text(x+.16, y+1.48, zh, fontsize=14, weight="bold", color=color)
    ax.text(x+.16, y+1.10, en, fontsize=10.5, color=color)
    ax.text(x+.16, y+.49, detail, fontsize=10.5, color=MUTED, linespacing=1.7, va="center")
for y in (4.45, 1.45):
    arrow(ax, 3.6, y, 4.35, y)
    arrow(ax, 7.65, y, 8.4, y)
ax.plot([10.05, 10.05, 1.95, 1.95], [3.48, 2.9, 2.9, 2.46],
        color=MUTED, linewidth=1.3)
arrow(ax, 1.95, 2.48, 1.95, 2.35)
ax.text(6, .08, "人确定问题与取舍；agent 检查、修改、实验 / Human judgment + agent execution",
        ha="center", fontsize=11, color=INK)
save(fig, "agent-build-investigation")
fig, ax = canvas("从接口布局对照到依赖边界优化",
                 "Hitagi Engine · agent 协作实验过程 · 各阶段分别建立基线", height=7)
stages_zh = [
    (.35, 3.55, "1  接口布局对照", "固定资源依赖设计\n比较分区与合并布局", BLUE),
    (4.4, 3.55, "2  跨目标复用诊断", "捕获参数差异\n检查 BMI 与对象编译次数", BLUE),
    (8.45, 3.55, "3  重新检验归因", "重复工作已消除\n布局耗时差距仍约 34 秒", ORANGE),
    (.35, .55, "4  关键路径分析", "trace 定位重复声明处理\n关闭 trace 后分项计时", TEAL),
    (4.4, .55, "5  interop 边界迁移", "集中第三方接口\n验证宏的调用点语义", TEAL),
    (8.45, .55, "6  配置审计与回归", "检查历史选项的必要性\n保留最小复现与测试", TEAL),
]
for x, y, title, detail, color in stages_zh:
    box(ax, x, y, 3.2, 1.85, title, detail, color)
for y in (4.45, 1.45):
    arrow(ax, 3.6, y, 4.35, y)
    arrow(ax, 7.65, y, 8.4, y)
ax.plot([10.05, 10.05, 1.95, 1.95], [3.48, 2.9, 2.9, 2.46], color=MUTED, linewidth=1.3)
arrow(ax, 1.95, 2.48, 1.95, 2.35)
ax.text(6, .08, "开发者约束架构与实验变量；agent 执行审计、修改和验证",
        ha="center", fontsize=11, color=INK)
save(fig, "agent-build-investigation-zh")

fig, ax = canvas("第三方声明的编译边界",
                 "示意图 · 箭头表示构建前置依赖 · 不表示实际耗时")
ax.text(.3, 5.64, "迁移前：各业务分区分别处理重型头文件", color=INK, fontsize=15, weight="bold")
for i, name in enumerate(("资源分区", "命令分区", "渲染图分区")):
    box(ax, .4+i*4, 4.05, 3.2, 1.05, name, "文本包含第三方头文件", color=ORANGE)
ax.text(6, 3.62, "重复解析声明，并增加模块接口处理成本", ha="center", color=MUTED)
ax.axhline(3.22, color="#d2dde8", linewidth=1)
box(ax, .4, 1.15, 2.8, 1.3, "interop.*", "集中第三方声明\n提供可复用接口", color=TEAL)
for y, name in ((2.4, "资源分区"), (1.38, "命令分区"), (.36, "渲染图分区")):
    box(ax, 4.4, y, 2.5, .75, name)
    arrow(ax, 3.2, 1.8, 4.4, y+.375, TEAL)
box(ax, 8.5, 1.15, 2.9, 1.3, "引擎与编辑器", "下游模块编译")
for y in (2.775, 1.755, .735):
    arrow(ax, 6.9, y, 8.5, 1.8)
ax.text(6, -.02, "减少重复工作，不消除引擎自身的依赖链", ha="center", color=INK, fontsize=11)
save(fig, "module-build-graph-zh")
print("Rendered seven figures as PNG and SVG.")
