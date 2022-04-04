参考了鲨鱼哥的 [librf](https://github.com/tearshark/librf) ，实现了go style的协程。加上Channel，使用起来已经很接近go了。

先看一下使用样例

```cpp
int main() {
    // 创建一个 Channel
    Channel<int> chan(1);
    // 开启一个协程往 Channel 里发送数据
    Go {
        for (int i = 0; i < 10; ++i) {
            chan << i;
            LOG_DEBUG << "send: " << i;
        }
        chan.close();
    };
    // 开启一个协程从 Channel 里读取数据
    Go {
        int i;
        while (chan >> i) {
            LOG_DEBUG << "recv: " << i;
        }
    };
}
```

可以看到非常容易上手，使用go关键字就能起一个协程。

现在看看具体该如何实现。

最原始调度器提交协程任务的方法为

```cpp
template<class FiberOrCb>
Scheduler* submit(FiberOrCb&& fc) {
    ...
}
```

按照这样提交，可以看到过程比较冗余，也不方便与宏配合。

```cpp
Scheduler::GetThis()->submit(fiber);
```

所以再重载一个运算符来简化一下


```cpp
// 重载+运算符，将任务转发给submit
template<class FiberOrCb>
Scheduler* operator+(FiberOrCb&& fc) {
    return submit(std::move(fc));
}
```

就可以这样提交协程了

```cpp
(*Scheduler::GetThis()) + fiber
```

由于前面部分都是固定的，可以用宏了简化一下

```cpp
#define go (*acid::IOManager::GetThis()) +
```

现在就能用go来启动函数或者协程了

```cpp
// 普通函数
go normal;
// 协程
go fiber;

// 函数对象
go [] {
    LOG_DEBUG << "Lambda";
};

std::string str = "hello";
// 但有个问题，捕获后变量是const，所以这个操作是错误，无法编译
go [str] {
    str += "hello";
};
// 要想改变变量，得加上mutable
go [str]() mutable {
    str += "world";
};
```

注意到最后一种情况有很强的通用性，即捕获变量进行修改，于是再设计一个宏来简化这种情况

```cpp
// Go 关键字默认按值捕获变量
// 等价于 go [=]() mutable {}
#define Go  (*acid::IOManager::GetThis()) + [=]()mutable
```

现在如果要捕获变量到协程里修改，就可以用Go来启动

```cpp
std::string str = "hello";

Go {
    str += "world";
};
```

注意Go默认按值捕获全部局部变量，所以使用起来要注意，如果变量太多就用go来按需捕获。

提供了配置默认调度器线程数量和调度器名字的方法，在还没启动go之前，可以使用如下方法设置

```cpp
// 设置默认调度器的线程数量
Config::Lookup<size_t>("scheduler.threads")->setValue(2);
// 设置默认调度器的名字
Config::Lookup<std::string>("scheduler.name")->setValue("default");
```

最新代码已经更新到GitHub上了

[zavier-wong/acid: A high performance fiber RPC network framework. 高性能协程RPC网络框架 (github.com)](https://github.com/zavier-wong/acid)
