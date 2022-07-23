# Hitagi Engine

Hitagi Engine 是以 C++20 开发的实验性游戏引擎，基本架构参考[从零开始手敲次世代游戏引擎](https://zhuanlan.zhihu.com/p/28587092)系列文章。目前能够完成基本的渲染功能，以及注册键盘鼠标监听事件。

## 基本架构
- 核心 (Core)
    - 使用 pmr 实现的，类 TCMalloc 的内存分配器（缺失 span 的实现）
    - 简答的文件读写模块
    - 简易线程池-
- 数学库
  - 3D相关的数学库，支持swizzle操作，以ISPC加速运算（需安装 [Intel SPMD Program Compiler](https://github.com/ispc/ispc) ）
    - 矩阵以行主序储存（无论是 CPU 侧，还是 GPU 侧。）
- 资源管理
    - 以 Assimp 作为模型解析，并导入到 AssetManager 中
- 图形接口模块
    - 图形接口抽象化
    - DX12中间层(进行中)
    - Render Graph (进行中)
- HID
- GUI
    - 使用 Dear ImGui
- 平台相关层
    - Windows


## 编译

因为目前只支持 DX12 ，因此这里只介绍如何在 Windows 下编译。

### 环境准备
首先需要安装 Visual Studio 对 C++ 的开发支持。一般来说，安装完 Visual Studio IDE 后就已经带有 C++ 的开发环境。
下面介绍无需安装 IDE 的环境准备。

1. 首先安装 [Visual Studio Installer](https://visualstudio.microsoft.com/downloads/) ，然后在 Visual Studio Installer 中勾选
    - MSVC v142 （可以的话尽可能最新）
    - C++ Clang-cl 生成工具, 用于 Clang 编译
    - C++ 核心功能、 C++ 生成工具核心功能
    - Windows 10 SDK （尽量最新）
    - Windows C 通用运行时
2. 安装 XMake ，用于构建整个项目， 具体安装方式请访问 [XMake 安装](https://xmake.io/#/zh-cn/guide/installation)，
3. 安装 [Clang14](https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.6/LLVM-14.0.6-win64.exe)

### 编译步骤
首先 Clone 此项目，并进入项目目录中
```bash
git clone https://github.com/L-Sun/HitagiEngine
cd HitagiEngine
```

运行如下命令进行构建，期间可能需要安装依赖，请保证网络通畅
```bash
xmake f -m debug # 可以切换为 release
```

#### 使用 Clang 进行编译
首先需要保证所有依赖使用 MSVC 安装完成（因为有些依赖无法使用 clang 编译），运行下面命令即可
```bash
xmake f -m debug -c # 保证依赖由 MSVC 编译
xmake f -m debug --cc=clang --cxx=clang++ # 切换为 clang 进行编译
xmake
```

### 启动
在 `examples` 目录下有部分简单的演示代码，可以在编译完成后，使用下面命令启动
```bash
xmake r demo
```