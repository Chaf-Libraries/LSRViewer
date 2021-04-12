# LSRViewer
Large  Scene Rendering Viewer

## 测试平台

* Windows 10专业版
* CPU: Intel(R) Core(TM) i9-10900 CPU @ 2.80GHz

* GPU: NVIDIA GeForce RTX 3060

## 开发环境

* Visual Studio Community 2019
* CMake 3.20.0
* Python 3.9.2
* Vulkan 1.0

## 开源依赖

* [Vulkan](https://github.com/SaschaWillems/Vulkan)
* [CTPL](https://github.com/vit-vit/CTPL)
* [entt](https://github.com/skypjack/entt)

## 构建方法

1. Windows操作系统
2. 安装Visual Studio Community 2019及CMake 3.20.0、Vulkan
3. 运行根目录下`build.bat`脚本即可

## Feature

* Basic Render Pipeline
* Scene Graph (still improving)
* CPU & GPU culling

## TODO

- [ ] Indirect Draw
- [ ] Sparse Virtual Texture
- [ ] Stream Loading
- [ ] Cluster Occlusion Culling

