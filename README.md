# Hitagi Engine
Hitagi Engine是以C++20开发的实验性游戏引擎，基本架构参考[从零开始手敲次世代游戏引擎](https://zhuanlan.zhihu.com/p/28587092)系列文章。目前能够完成基本的渲染功能，以及注册键盘鼠标监听事件。

![](/doc/src/demo.png)

## 完成模块
### 核心
1. 图形学相关的数学库，支持swizzle操作，以ISPC加速运算
2. 类TCMalloc的内存分配器
3. 文件读写模块

### 场景管理
以Assimp作为模型解析，并导入到SceneManager中

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
1. MSVC v142
2. C++ Clang-cl生成工具
3. C++ 核心功能、C++生成工具核心功能
4. Windows 10 SDK
5. Windows C 通用运行时

并安装Vcpkg，具体方式请访问 [https://github.com/Microsoft/vcpkg](https://github.com/Microsoft/vcpkg)


此时在Vcpkg中以x64-windows下载安装如下第三方库
1. crossguid
2. zlib
3. libjpeg-turbo
4. libpng
5. assimp
6. freetype
7. fmt
8. spdlog
9. gtest

命令为
```
vcpkg.exe install --triplet x64-windows crossguid zlib libjpeg-turbo libpng assimp freetype fmt spdlog gtest
```

Clone此项目
```
git clone https://github.com/L-Sun/HitagiEngine
```
，并在`.vscode/settings.json`中修改`E:/workspace/vcpkg`为你所安装Vcpkg的具体路径，并删除`http.proxy`一整项。

使用VSCode打开此项目，并在CMake Tool中设置编译链为`用于 MSVC 的 Clang 11.0.0，具有 Visual Studio 生成工具 2019 Release (amd64)`
此时即可开始编译此项目。

输出文件位于build文件夹中，其中MyTest.exe为主程序。