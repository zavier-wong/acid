# ACID: 分布式服务治理框架

> 学习本项目需要有一定的C++，RPC，分布式知识
### 构建项目 / Build
1.项目用到了大量C++17/20新特性，如`constexpr if`的编译期代码生成，基于c++20 `coroutine`的无栈协程状态机解析 URI 和 HTTP 协议等。注意，必须安装g++-11，否则不支持编译。

ubuntu可以采用以下方法升级g++，对于其他环境，可以下载源代码自行编译。
```shell
sudo apt install software-properties-common
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt install gcc-11 g++-11
```
2.构建项目 / Build
```shell
git clone --recursive https://github.com/zavier-wong/acid
sudo bash build.sh
```
现在你可以在 acid/lib 里面看见编译后的静态库 libacid.a

如果有对库修改，重新编译只需
```shell
cd build
cmake ..
make
```

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