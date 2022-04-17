# Hitagi Engine
Hitagi Engine是以C++20开发的实验性游戏引擎，基本架构参考[从零开始手敲次世代游戏引擎](https://zhuanlan.zhihu.com/p/28587092)系列文章。目前能够完成基本的渲染功能，以及注册键盘鼠标监听事件。

> 目前正在重构并迁移至 [xmake](https://github.com/xmake-io/xmake)构建系统，位于xmake分支。

![](/doc/src/demo.png)

## 完成模块
### 核心

1. 图形学相关的数学库，支持swizzle操作，以ISPC加速运算（需安装 [Intel SPMD Program Compiler](https://github.com/ispc/ispc)）
    1. 矩阵以行主序储存（无论是CPU侧，还是GPU侧。dxc编译时候，需要加上 `-Zpr` 表示行主序）
2. 使用pmr实现的，类TCMalloc的内存分配器（缺失span的实现）
3. 文件读写模块

### 场景管理
1. 以Assimp作为模型解析，并导入到SceneManager中
2. BVH骨骼运动文件解析

### 图形接口模块
1. 图形接口抽象化
2. DX12中间层(进行中)
3. RenderGraph(进行中)

### HID
1. 鼠标键盘事件注册

## 开发环境
在编译项目前，由于GFW的原因，可能在安装部分库时出现网络错误，请确保网络通畅。以下为编译时所需环境，以WIN10为例
1. Visual Studio 生成工具 2019 (v16.8.3)
2. Clang ([下载地址](http://llvm.org/releases/download.html))
3. CMake v3.15及以上
4. VSCode、(插件：CMake Tools)
5. Vcpkg

## 编译准备
在Visual Studio Installer 中勾选
1. MSVC v142 （可以的话尽可能最新）
2. C++ Clang-cl生成工具
3. C++ 核心功能、C++生成工具核心功能
4. Windows 10 SDK
5. Windows C 通用运行时

并安装Vcpkg，具体方式请访问 [https://github.com/Microsoft/vcpkg](https://github.com/Microsoft/vcpkg)

Clone此项目
```
git clone https://github.com/L-Sun/HitagiEngine
```
，并在`.vscode/settings.json`中加入vcpkg的路径，其中`/path/to/vcpkg`为vcpkg的安装路径
```json
    "cmake.configureArgs": [
        "-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
    ]
```

使用VSCode打开此项目，并在CMake Tool中设置编译链为`用于 MSVC 的 Clang 11.0.0，具有 Visual Studio 生成工具 2019 Release (amd64)`
此时即可开始编译此项目。

输出文件位于build文件夹中，其中MyTest.exe为主程序。
