# ACID: 分布式服务治理框架

> 学习本项目需要有一定的C++，RPC，分布式知识

### 注意

项目经过了重构，如果你是阅读文章而来的读者，可以前往
[旧版本](https://github.com/zavier-wong/acid/tree/0f45acff75979d4636d217b78cd61fc2b1c01751)


### 构建项目 / Build
1. 项目用到了大量C++17/20新特性，如`constexpr if`的编译期代码生成，基于c++20 `coroutine`的无栈协程状态机解析 URI 和 HTTP 协议等。注意，必须安装g++-11，否则不支持编译。

ubuntu可以采用以下方法升级g++，对于其他环境，可以下载源代码自行编译。
```shell
sudo apt install software-properties-common
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt install gcc-11 g++-11
```
2. 构建依赖库
```shell
git clone --recursive https://github.com/zavier-wong/acid
cd acid
sudo bash build.sh
```
3.构建项目 / Build
```shell
mkdir build
cd build
cmake ..
make
cd ..
```
现在你可以在 acid/lib 里面看见编译后的静态库 libacid.a

example 和 tests 的 cmake 已经写好，只需要进入对应的目录
```shell
mkdir build
cd build
cmake ..
make
```

## 反馈和参与

* bug、疑惑、修改建议都欢迎提在[Github Issues](https://github.com/zavier-wong/acid/issues)中
* 也可发送邮件 [acid@163.com](mailto:acid@163.com) 交流学习
