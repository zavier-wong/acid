# 分布式 KV 存储

## 架构
启动三个 http 节点组成一个 kv 集群，默认绑定以下地址
```
localhost:10001
localhost:10002
localhost:10003
```
启动方法
```shell
./test_kvserver 1
./test_kvserver 2
./test_kvserver 3
```


## 文档说明

- 每个response都有msg参数，其中，只有msg为OK，其他参数才有效

- API 使用 POST 请求，向服务器发送json数据

    
## 请求与响应规范

- 请求示例

  ```json
  {
    "command": "put",
    "key": "",
    "value": ""
  }

  ```

- 返回示例

  ```json
  {
    "msg": "OK",
    "value": "",
    "data": {}
  }
  ```

- 返回msg

    - OK ----正常返回

    - NO_KEY ----没有该key

    - WRONG_LEADER ----请求的http server不是kv集群的leader
  
    - TIMEOUT ----请求超时
  
    - 其他 msg

## API详细说明

#### 查询一条数据

```
POST	/kv
// 请求参数
{
    "command": "get",
    "key": "key"
}
// 响应参数
{
    "msg": "OK",
    "value": ""
}
```

#### 查询全部数据

```
POST	/kv
// 请求参数
{
    "command": "dump"
}
// 响应参数
{
    "msg": "OK",
    "data": {
        "key1": "value1",
        "key2": "value2",
        ...
    }
}
```
#### 覆盖数据

```
POST	/kv
// 请求参数
{
    "command": "put",
    "key": "key"，
    "value": "value"
}
// 响应参数
{
    "msg": "OK"
}
```
#### 追加数据

```
POST	/kv
// 请求参数
{
    "command": "put",
    "key": "key"，
    "value": "value"
}
// 响应参数
{
    "msg": "OK"
}
```
#### 删除数据

```
POST	/kv
// 请求参数
{
    "command": "delete",
    "key": "key"
}
// 响应参数
{
    "msg": "OK"
}
```