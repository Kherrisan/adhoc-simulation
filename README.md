# adhoc-simulation

## 介绍

本项目用于模拟adhoc网络中的节点通信，并将在此基础上实现模拟拓扑网络和路由功能。

## 配置开发环境

本项目使用CMake来生成辅助项目构建，建议使用CLion进行代码的开发和调试。

### Boost::asio

本项目底层cs通信模块依赖了Boost库中的asio库以实现client、server端的socket和thread的管理，要运行本项目首先需要下载[Boost库](https://www.boost.org/users/download/) 。下载完成后解压到本地任意一个目录内，**记住目录绝对地址**。

### CLion

安装[CLion IDE](https://www.jetbrains.com/clion/) 。MacOS/Linux下应该是不需要配置IDE的编译工具链的，在Windows下需要配置，配置过程可参考[Windows上CLion配置和使用教程](https://blog.csdn.net/lu_linux/article/details/88713355) 。

### CMakeLists

拷贝一份CMakeLists.example.txt到当前目录并重命名为CMakeList.txt，修改其中到Boost库头文件位置。

```cmake
set(Boost_INCLUDE_DIR /Users/xxx/boost_1_77_0)
```

然后CLion会自动索引Boost库的代码（此过程会花费很长时间），在索引完成之后就可以编译并运行server和client这两个目标文件了。

### 可能会遇到的问题



## 目录结构

- CMakeLists.example.txt：cmake编译器的配置文件，其中包含了对boost库地址对引用。**若要运行请先重命名为CMakeLists.txt并修改boost库的地址。**
- client_main.cpp：cs通信demo的client入口。包含client的启动和发消息流程的代码。
- server_main.cpp：cs通信demo的server入口。包含server的启动流程的代码。
- message.h：通信消息message类的定义。包含数据的字节表示、编码、解码功能的实现。
- server.h：cs通信server端的实现。
- client.h：cs通信client端的实现。

## 规范

git commit消息建议尽可能使用[Git Commit Message Conventions](https://docs.google.com/document/d/1QrDFcIiPjSLDn3EL15IJygNPiHORgU1_OOAqWjiDU5Y/edit#heading=h.t7ifoyph8bd3) 或者[Commit message 和 Change log 编写指南](https://www.ruanyifeng.com/blog/2016/01/commit_message_change_log.html) 。

