## Gomoku-网络在线五子棋


### 环境配置
ubuntu24.04环境，安装lrzsz、jsoncpp、gcc/g++、cmake、makefile、websocket、gdb、git、boost

- jsoncpp安装：sudo apt-get install libjsoncpp-dev
- boost安装：sudo apt-get install libboost-all-dev
- websocket安装：git clone https://github.com/zaphoyd/websocketpp.git 后新建build，cmake后安装到系统目录

### Websocket介绍

从 HTML5 开始支持的一种网页端和服务端保持长连接的消息推送机制。

- 传统的 web 程序都是属于 "一问一答" 的形式，即客户端给服务器发送了一个 HTTP 请求，服务器给客户端返回一个 HTTP 响应。这种情况下服务器是属于被动的一方，如果客户端不主动发起请求服务器就无法主动给客户端响应
- 像网页即时聊天非常依赖 "消息推送" 的， 即需要服务器主动推动消息到客户端。如果只是使用原生的 HTTP 协议，要想实现消息推送一般需要通过 "轮询" 的方式实现， 而轮询的**成本比较高并且也不能及时的获取到消息的响应**。
- WebSocket 更接近于 TCP 这种级别的通信方式，一旦连接建立完成客户端或者服务器都可以**主动的向对方发送数据**。

#### 原理介绍
WebSocket 协议本质上是一个基于 TCP 的协议。为了建立一个 WebSocket 连接，客户端浏览器首先要向服务器发起一个 HTTP 请求，这个请求和通常的 HTTP 请求不同，包含了一些附加头信息，通过这个附加头信息完成握手过程并升级协议的过程。

协议升级大致步骤如下：



客户端（如浏览器）首先发送一个**特殊的HTTP GET请求**，明确告知服务器“希望升级到WebSocket协议”。这个请求包含以下关键头部字段：

| 头部字段               | 作用                                                                 |
|------------------------|----------------------------------------------------------------------|
| Upgrade: websocket | 声明要升级的目标协议为WebSocket（固定值）。                           |
| Connection: Upgrade | 配合Upgrade字段，表明这是一个协议升级请求（固定值）。               |
| Sec-WebSocket-Key  | 客户端生成的随机16字节值（经Base64编码），用于服务器验证和握手。     |
| Sec-WebSocket-Version| 客户端支持的WebSocket版本（当前标准为13，必须指定）。                 |
| 


服务器收到请求后，会检查是否支持WebSocket协议及对应版本。如果同意升级，会返回一个101 Switching Protocols 响应，表明协议切换成功。响应包含以下关键头部：

| 头部字段               | 作用                                                                 |
|------------------------|----------------------------------------------------------------------|
| Upgrade: websocket | 确认升级到WebSocket协议（固定值）。                                   |
| Connection: Upgrade  | 确认协议升级（固定值）。                                             |
| Sec-WebSocket-Accept | 服务器对客户端Sec-WebSocket-Key的验证结果。             |
|



#### 报文大致介绍：



WebSocket帧的头部由多个二进制位（bit）和字节组成，结构如下（从左到右，高位到低位）：

| 字段               | 长度（bit） | 作用                                                                 |
|--------------------|-------------|----------------------------------------------------------------------|
| **FIN**            | 1           | 标记当前帧是否是“消息的最后一帧”（1=最后一帧；0=后续还有帧，用于消息分片）。 |
| **RSV1~RSV3**      | 1+1+1       | 保留位，用于扩展协议（默认必须为0，若使用扩展需提前协商）。             |
| **Opcode**         | 4           | 帧类型标识（区分数据帧、控制帧）。                                   |
| **Mask**           | 1           | 标记 payload 数据是否经过掩码处理（1=客户端发送的帧，必须掩码；0=服务器发送的帧，无掩码）。 |
| **Payload Length** | 7 / 7+16 / 7+64 | 负载数据（实际内容）的长度（分3种表示方式）。                         |
| **Masking Key**    | 0 / 32      | 掩码密钥（仅当Mask=1时存在，共4字节，用于解密客户端发送的数据）。     |
| **Payload Data**   | 可变        | 实际传输的数据（文本、二进制或控制信息），长度由Payload Length指定。  |



1. **FIN（1 bit）**  
用于支持“消息分片”：一个大消息可拆分为多个帧发送，只有最后一帧的FIN=1，其他帧FIN=0。  



1. **Opcode（4 bits）**  
定义帧的类型，核心取值如下：  
- **0x0**：延续帧（用于消息分片，承接上一帧的数据）。  
- **0x1**：文本帧（payload为UTF-8编码的文本数据，如JSON字符串）。  
- **0x2**：二进制帧（payload为二进制数据，如图片、音频）。   

3. **Payload Length（7/7+16/7+64 bits）**  
表示Payload Data的长度，分3种情况：  
- 若值 **≤125**：直接表示长度（7 bits足够，无需额外字节）。  
- 若值 = **126**：后续2字节（16 bits）表示实际长度（范围126~65535）。  
- 若值 = **127**：后续8字节（64 bits）表示实际长度（范围65536~2^64-1）。  


#### 常见的Websocket接口介绍：
```c++
namespace websocketpp {
    typedef lib::weak_ptr<void> connection_hdl;
    template <typename config>
    class endpoint : public config::socket_type {
    typedef lib::shared_ptr<lib::asio::steady_timer> timer_ptr;
    typedef typename connection_type::ptr connection_ptr;
    typedef typename connection_type::message_ptr message_ptr;
    typedef lib::function<void(connection_hdl)> open_handler;
    typedef lib::function<void(connection_hdl)> close_handler;
    typedef lib::function<void(connection_hdl)> http_handler;
    typedef lib::function<void(connection_hdl,message_ptr)> message_handler;
    /* websocketpp::log::alevel::none 禁止打印所有日志*/
    void set_access_channels(log::level channels);/*设置日志打印等级*/
    void clear_access_channels(log::level channels);/*清除指定等级的日志*/
    /*设置指定事件的回调函数*/
    void set_open_handler(open_handler h);/*websocket握手成功回调处理函数*/
    void set_close_handler(close_handler h);/*websocket连接关闭回调处理函数*/
    void set_message_handler(message_handler h);/*websocket消息回调处理函数*/
    void set_http_handler(http_handler h);/*http请求回调处理函数*/
    /*发送数据接口*/
    void send(connection_hdl hdl, std::string& payload,
    frame::opcode::value op);
    void send(connection_hdl hdl, void* payload, size_t len,
    frame::opcode::value op);
    /*关闭连接接口*/
    void close(connection_hdl hdl, close::status::value code, std::string&
    reason);
    /*获取connection_hdl 对应连接的connection_ptr*/
    connection_ptr get_con_from_hdl(connection_hdl hdl);
    /*websocketpp基于asio框架实现，init_asio用于初始化asio框架中的io_service调度
    器*/
    void init_asio();
    /*设置是否启用地址重用*/
    void set_reuse_addr(bool value);
    /*设置endpoint的绑定监听端口*/
    void listen(uint16_t port);
    /*对io_service对象的run接口封装，用于启动服务器*/
    std::size_t run();
    /*websocketpp提供的定时器，以毫秒为单位*/
    timer_ptr set_timer(long duration, timer_handler callback);
};

template <typename config>
    class server : public endpoint<connection<config>,config> {
/*初始化并启动服务端监听连接的accept事件处理*/
    void start_accept();
}；

template <typename config>
class connection
    : public config::transport_type::transport_con_type
    , public config::connection_base
{
    /*发送数据接口*/
    error_code send(std::string&payload, frame::opcode::valueop=frame::opcode::text);
    /*获取http请求头部*/
    std::string const & get_request_header(std::string const & key)
    /*获取请求正文*/
    std::string const & get_request_body();
    /*设置响应状态码*/
    void set_status(http::status_code::value code);
    /*设置http响应正文*/
    void set_body(std::string const & value);
    /*添加http响应头部字段*/
    void append_header(std::string const & key, std::string const & val);
    /*获取http请求对象*/
    request_type const & get_request();
    /*获取connection_ptr 对应的 connection_hdl */
    connection_hdl get_handle();
};
namespace http {
    namespace parser {
    class parser {
        std::string const & get_header(std::string const & key)
        }
    class request : public parser {
        /*获取请求方法*/
        std::string const & get_method()
            /*获取请求uri接口*/
        std::string const & get_uri()
    };
}};
namespace message_buffer {
    /*获取websocket请求中的payload数据类型*/
    frame::opcode::value get_opcode();
    /*获取websocket中payload数据*/
    std::string const & get_payload();
};
namespace log {
    struct alevel {
    static level const none = 0x0;
    static level const connect = 0x1;
    static level const disconnect = 0x2;
    static level const control = 0x4;
    static level const frame_header = 0x8;
    static level const frame_payload = 0x10;
    static level const message_header = 0x20;
    static level const message_payload = 0x40;
    static level const endpoint = 0x80;
    static level const debug_handshake = 0x100;
    static level const debug_close = 0x200;
    static level const devel = 0x400;
    static level const app = 0x800;
    static level const http = 0x1000;
    static level const fail = 0x2000;
    static level const access_core = 0x00003003;
    static level const all = 0xffffffff;
    };
}
namespace http {
    namespace status_code {
        enum value {
        uninitialized = 0,
        continue_code = 100,
        switching_protocols = 101,
        ok = 200,
        created = 201,
        accepted = 202,
        non_authoritative_information = 203,
        no_content = 204,
        reset_content = 205,
        partial_content = 206,
        multiple_choices = 300,
        moved_permanently = 301,
        found = 302,
        see_other = 303,
        not_modified = 304,
        use_proxy = 305,
        temporary_redirect = 307,
        bad_request = 400,
        unauthorized = 401,
        payment_required = 402,
        forbidden = 403,
        not_found = 404,
        method_not_allowed = 405,
        not_acceptable = 406,
        proxy_authentication_required = 407,
        request_timeout = 408,
        conflict = 409,
        gone = 410,
        length_required = 411,
        precondition_failed = 412,
        request_entity_too_large = 413,
        request_uri_too_long = 414,
        unsupported_media_type = 415,
        request_range_not_satisfiable = 416,
        expectation_failed = 417,
        im_a_teapot = 418,
        upgrade_required = 426,
        precondition_required = 428,
        too_many_requests = 429,
        request_header_fields_too_large = 431,
        internal_server_error = 500,
        not_implemented = 501,
        bad_gateway = 502,
        service_unavailable = 503,
        gateway_timeout = 504,
        http_version_not_supported = 505,
        not_extended = 510,
        network_authentication_required = 511
    };}
}

namespace frame {
    namespace opcode {
        enum value {
        continuation = 0x0,
        text = 0x1,
        binary = 0x2,
        rsv3 = 0x3,
        rsv4 = 0x4,
        rsv5 = 0x5,
        rsv6 = 0x6,
        rsv7 = 0x7,
        close = 0x8,
        ping = 0x9,
        pong = 0xA,
        control_rsvb = 0xB,
        control_rsvc = 0xC,
        control_rsvd = 0xD,
        control_rsve = 0xE,
        control_rsvf = 0xF,
    };}
}
} 
```

#### 使用websocketpp搭建服务器的流程：

1. 实例化server对象
2. 设置日志输出等级
3. 初始化asio框架种的调度器
4. 设置业务处理回调函数（具体业务处理的函数由我们自己实现）
5. 设置服务器监听端口
6. 开始获取新建连接
7. 启动服务器

### Jsoncpp介绍

这里使用jsoncpp库来进行序列化和反序列化，常用接口如下：
```c++
class Json::Value{
    Value &operator=(const Value &other); //Value重载了[]和=，因此所有的赋值和获取数据都可以通过
    Value& operator[](const std::string& key);//简单的方式完成 val["name"] ="xx";
    Value& operator[](const char* key);
    Value removeMember(const char* key);//移除元素
    const Value& operator[](ArrayIndex index) const; //val["score"][0]
    Value& append(const Value& value);//添加数组元素val["score"].append(88);
    ArrayIndex size() const;//获取数组元素个数 val["score"].size();
    bool isNull(); //用于判断是否存在某个字段
    std::string asString() const;//转string string name = val["name"].asString();
    const char* asCString() const;//转char* char *name = val["name"].asCString();
    Int asInt() const;//转int int age = val["age"].asInt();
    float asFloat() const;//转float float weight = val["weight"].asFloat();
    bool asBool() const;//转 bool bool ok = val["ok"].asBool();
};
```
Jsoncpp 库主要借助三个类以及其对应的少量成员函数完成序列化及反序列化：
```c++
//序列化接口
class JSON_API StreamWriter {
virtual int write(Value const& root, std::ostream* sout) = 0;
}
class JSON_API StreamWriterBuilder : public StreamWriter::Factory {
virtual StreamWriter* newStreamWriter() const;
}

//反序列化接口
class JSON_API CharReader {
virtual bool parse(char const* beginDoc, char const* endDoc, Value* root, std::string* errs) = 0;
}
class JSON_API CharReaderBuilder : public CharReader::Factory {
virtual CharReader* newCharReader() const;
}
```
  
1. Json::Value：用于在内存中存储JSON数据（支持对象、数组、字符串、数字等所有JSON类型）。  
2. Json::StreamWriterBuilder + Json::StreamWriter：用于将Json::Value中的数据**序列化**为JSON字符串。  
3. Json::CharReaderBuilder + Json::CharReader：用于将JSON字符串**反序列化**为Json::Value对象。  


#### 序列化：
#### 1. 构建内存数据结构（`Json::Value`）
```cpp
Json::Value root;  // 根节点，默认是"null"，后续赋值后会自动转换类型
root["name"] = "Bob";  // 添加字符串类型键值对（root变为JSON对象{}）
root["age"] = 18;      // 添加整数类型键值对
root["is_student"] = true;  // 添加布尔类型键值对
// 添加数组类型键值对：通过append()向"score"键对应的数组添加元素
root["score"].append(66.66);  // 数组元素1（浮点数）
root["score"].append(77.77);  // 数组元素2
root["score"].append(88.88);  // 数组元素3
```
- `Json::Value`是“动态类型”的：初始为`null`，当添加键值对后自动变为**JSON对象**（`{}`）；当调用`append()`时，自动变为**JSON数组**（`[]`）。  
- 支持多种数据类型：字符串（`std::string`）、整数（`int`）、布尔（`bool`）、浮点数（`double`），以及嵌套的对象/数组。  


#### 2. 将`Json::Value`序列化为JSON字符串
```cpp
// 1. 创建流写入器工厂（配置序列化参数，如缩进、格式等，默认即可）
Json::StreamWriterBuilder swb;  
// 2. 通过工厂创建流写入器（实际执行序列化的对象）
Json::StreamWriter* writer = swb.newStreamWriter();  
// 3. 创建字符串流（用于存储序列化后的JSON字符串）
stringstream ss;  
// 4. 执行序列化：将root中的数据写入ss（stringstream）
if (writer->write(root, &ss) != 0) {  
    cerr << "json wrong" << endl;  
    return;  
}  
// 5. 从ss中获取JSON字符串（序列化结果）
cout << ss.str() << endl;  // 输出：{"age":18,"is_student":true,"name":"Bob","score":[66.66,77.77,88.88]}
// 6. 释放资源（避免内存泄漏）
delete writer;  
```
- `StreamWriterBuilder`是“工厂类”：负责创建`StreamWriter`，并可通过配置参数（如`["indentation"] = "  "`）控制JSON字符串的格式（如缩进空格数）。  
- `StreamWriter::write()`是核心方法：接收`Json::Value`（待序列化数据）和`std::ostream*`（输出流，如`stringstream`、文件流`ofstream`等），将数据转换为JSON字符串并写入流中。  


#### 反序列化

#### 1. 将JSON字符串反序列化为`Json::Value`
```cpp
// 1. 创建字符读取器工厂（配置反序列化参数，默认即可）
Json::CharReaderBuilder crb;  
// 2. 通过工厂创建字符读取器（实际执行反序列化的对象）
Json::CharReader* reader = crb.newCharReader();  
// 3. 声明用于存储反序列化结果的Json::Value对象
Json::Value value;  
// 4. 声明用于存储错误信息的字符串（若反序列化失败）
string err;  
// 5. 执行反序列化：解析str中的JSON字符串，结果存入value
if (reader->parse(str.c_str(), str.c_str() + str.size(), &value, &err) == false) {  
    cerr << "parse wrong" << endl;  
    return;  
}  
// 6. 释放资源
delete reader;  
```
- `CharReaderBuilder`是“工厂类”：负责创建`CharReader`，用于解析JSON字符串。  
- `CharReader::parse()`是核心方法：  
  - 参数1/2：JSON字符串的起始/结束指针（通过`str.c_str()`和`str.c_str() + str.size()`指定完整范围）；  
  - 参数3：接收反序列化结果的`Json::Value*`；  
  - 参数4：接收错误信息的`std::string*`（若解析失败，会存入具体错误原因，如“语法错误”“括号不匹配”等）。  


#### 3. 从反序列化结果中读取数据
```cpp
// 读取字符串类型（name）
cout << value["name"].asString() << endl;  // 输出：Bob  
// 读取整数类型（age）
cout << value["age"].asInt() << endl;      // 输出：18  
// 读取布尔类型（is_student）
cout << value["is_student"].asBool() << endl;  // 输出：1（true在C++中输出为1）  
// 读取数组类型（score）：通过size()获取数组长度，[]访问元素
for (int i = 0; i < value["score"].size(); i++) {  
    cout << value["score"][i].asDouble() << " ";  // 输出：66.66 77.77 88.88  
}  
```
- `Json::Value`提供了一系列`asXxx()`方法，用于将存储的数据转换为对应类型（如`asString()`、`asInt()`、`asDouble()`、`asBool()`等）。  
- 对于数组，通过`size()`获取元素个数，通过`[索引]`访问具体元素；对于对象，通过`[键名]`访问对应值。  


### Mysql接口介绍




#### 1. mysql_init（MySQL操作句柄初始化）
- **函数原型**：`MYSQL *mysql_init(MYSQL *mysql)`  
- **参数说明**：  mysql为空则动态申请句柄空间进行初始化  
- **返回值**：成功返回句柄指针，失败返回NULL  





#### 2. mysql_real_connect（连接mysql服务器）
- **函数原型**：`MYSQL *mysql_real_connect(MYSQL *mysql, const char *host, const char *user, const char *passwd, const char *db, unsigned int port, const char *unix_socket, unsigned long client_flag)`  
- **参数说明**：  
  - mysql：初始化完成的句柄  
  - host：连接的mysql服务器的地址  
  - user：连接的服务器的用户名  
  - passwd：连接的服务器的密码  
  - db：默认选择的数据库名称  
  - port：连接的服务器的端口：默认0是3306端口  
  - unix_socket：通信管道文件或者socket文件，通常置NULL  
  - client_flag：客户端标志位，通常置0  
- **返回值**：成功返回句柄指针，失败返回NULL  





#### 3. mysql_set_character_set（设置当前客户端的字符集）
- **函数原型**：`int mysql_set_character_set(MYSQL *mysql, const char *csname)`  
- **参数说明**：  
  - mysql：初始化完成的句柄  
  - csname：字符集名称，通常："utf8"  
- **返回值**：成功返回0，失败返回非0  





#### 4. mysql_select_db（选择操作的数据库）
- **函数原型**：`int mysql_select_db(MYSQL *mysql, const char *db)`  
- **参数说明**：  
  - mysql：初始化完成的句柄  
  - db：要切换选择的数据库名称  
- **返回值**：成功返回0，失败返回非0  





#### 5. mysql_query（执行sql语句）
- **函数原型**：`int mysql_query(MYSQL *mysql, const char *stmt_str)`  
- **参数说明**：  
  - mysql：初始化完成的句柄  
  - stmt_str：要执行的sql语句  
- **返回值**：成功返回0，失败返回非0  




#### 6. mysql_store_result（保存查询结果到本地）
- **函数原型**：`MYSQL_RES *mysql_store_result(MYSQL *mysql)`  
- **参数说明**：  
  mysql：初始化完成的句柄  
- **返回值**：成功返回结果集的指针，失败返回NULL  





#### 7. mysql_num_rows（获取结果集中的行数）
- **函数原型**：`uint64_t mysql_num_rows(MYSQL_RES *result)`  
- **参数说明**：  
  result：保存到本地的结果集地址  
- **返回值**：结果集中数据的条数  





#### 8. mysql_num_fields（获取结果集中的列数）
- **函数原型**：`unsigned int mysql_num_fields(MYSQL_RES *result)`  
- **参数说明**：  
  result：保存到本地的结果集地址  
- **返回值**：结果集中每一条数据的列数  





#### 9. mysql_fetch_row（遍历结果集）
- **函数原型**：`MYSQL_ROW mysql_fetch_row(MYSQL_RES *result)`  
- **参数说明**：  
  result：保存到本地的结果集地址；该接口会保存当前读取结果位置，每次获取的都是下一条数据  
- **返回值**：实际上是一个char **的指针，将每一条数据做成了字符串指针数组；row[0]-第0列，row[1]-第1列 ...  





#### 10. mysql_free_result（释放结果集）
- **函数原型**：`void mysql_free_result(MYSQL_RES *result)`  
- **参数说明**：  
  result：保存到本地的结果集地址  





#### 11. mysql_close（关闭数据库客户端连接，销毁句柄）
- **函数原型**：`void mysql_close(MYSQL *mysql)`  
- **参数说明**：  
  mysql：初始化完成的句柄  





#### 12. mysql_error（获取mysql接口执行错误原因）
- **函数原型**：`const char *mysql_error(MYSQL *mysql)`  
- **参数说明**：  
  mysql：初始化完成的句柄  


### CPP中互斥锁和条件变量


#### 1. 互斥锁相关（`#include <mutex>`）
| 组件/接口                | 功能说明                                                                 |
|--------------------------|--------------------------------------------------------------------------|
| `std::mutex`             | 基础互斥锁，提供`lock()`和`unlock()`方法，用于保护共享资源。             |
| `mutex.lock()`           | 获取锁（若已被其他线程占用，则阻塞等待）。                               |
| `mutex.unlock()`         | 释放锁（必须由持有锁的线程调用，否则行为未定义）。                       |
| `std::lock_guard<std::mutex>` | RAII风格的锁管理工具，构造时自动`lock()`，析构时自动`unlock()`，避免忘记释放锁导致死锁。 |
| `std::unique_lock<std::mutex>` | 更灵活的RAII锁管理，支持手动`lock()`/`unlock()`，可配合条件变量使用（因为条件变量`wait()`需要临时释放锁）。 |


#### 2. 条件变量相关（`#include <condition_variable>`）
| 接口                          | 功能说明                                                                 |
|-------------------------------|--------------------------------------------------------------------------|
| `std::condition_variable`     | 用于线程间同步，让线程等待某个条件满足后再执行。                         |
| `cv.wait(lock, predicate)`    | ① 释放`lock`持有的锁，阻塞当前线程；<br>② 被唤醒后重新获取锁，检查`predicate`（条件）；<br>③ 若条件为`true`则继续执行，否则再次阻塞（防止虚假唤醒）。 |
| `cv.notify_one()`             | 唤醒一个正在等待该条件变量的线程（若有多个，随机选一个）。               |
| `cv.notify_all()`             | 唤醒所有正在等待该条件变量的线程。                                       |


#### 注意
1. 条件变量必须与互斥锁配合使用，且`wait()`需传入`unique_lock`（因为要临时释放锁）。
2. `wait()`必须配合条件判断（`predicate`），否则可能因“虚假唤醒”（系统随机唤醒）导致逻辑错误。
3. 锁的粒度要尽可能小（只保护共享资源的读写），避免影响性能。


### 前端基础介绍



#### 一、HTML（超文本标记语言）
- **作用**：构建网页的**结构框架**，通过各类标签定义页面中的文字、图片、链接、表单、列表等元素。
- **代码体现**：
  - 标题：`<h1>HELLO WORLD</h1>` 定义一级标题。
  - 段落：`<p>第一段</p>` `<p>第二行<br/>第三行</p>` 定义文本段落，`<br/>` 实现换行。
  - 图片：`<img src='...' title="02"/>` 插入图片并设置提示文字。
  - 链接：`<a href="https://blog.csdn.net/Dai_renwen" target="_self">我的主页</a>` 定义跳转链接。
  - 列表：`<ul><li>无序列表项</li></ul>`（无序列表）、`<ol><li>有序列表项</li></ol>`（有序列表）、`<dl><dt>分类1</dt><dd>分类项1</dd></dl>`（定义列表）。
  - 表单：`<form>...</form>` 包裹输入框、密码框、提交按钮，用于数据提交。
  - 容器：`<div>独占一行</div>`（块级容器，独占一行）、`<span>多行</span>`（行内容器，可同行显示）。


#### 二、CSS（层叠样式表）
- **作用**：为HTML元素添加**视觉样式**（如颜色、字体、布局等），提升页面美观度。
- **代码体现**：
  代码中通过`<style>`标签定义内部样式表，示例：
  ```css
  p{
      color:blue;
  }
  ```
  作用是将所有`<p>`标签的文字颜色设置为蓝色，实现了对HTML结构的样式修饰。


#### 三、JS（JavaScript，脚本语言）
- **作用**：实现页面**交互逻辑**（如按钮点击响应、数据获取与修改、异步请求等）。
- **代码体现**：
  - **事件绑定**：`<button onclick='test()'>按钮</button>` 为按钮绑定`test`函数，点击时触发交互。
  - **DOM操作**：在`test`函数中，通过`document.getElementById("username")`获取输入框元素，进而获取或设置其内容（如`input.value`）。
  - **逻辑处理**：定义`login_info`对象，封装用户名和密码数据，为AJAX请求做准备。


#### 四、AJAX（异步HTTP请求）
- **作用**：在不刷新页面的前提下，**异步与服务器交互数据**（发送请求、接收响应），提升用户体验。
- **代码体现**：
  代码中借助jQuery的`$.ajax`方法实现异步请求：
  - 请求配置：`type:"post"`（请求方式）、`url:'http://124.222.53.34:8080/index.html'`（请求地址）、`data:JSON.stringify(login_info)`（将数据转为JSON字符串传递）。
  - 回调处理：`success`函数处理请求成功的响应，`error`函数处理请求失败的情况，实现了无需刷新页面的异步数据交互。

封装工具模块:
1. 日志宏: 实现程序日志打印
2. mysqltool: 数据库的连接&初始化，句柄的销毁，语句的执行
3. jsontool: 封装实现json的序列化和反序列化
4. stringtool: 主要是封装实现字符串分割功能。
5. filetool: 主要封装了文件数据的读取、写入功能（对于html文件数据进行读取）

### 数据库管理
#### 表设计
包含玩家序号、玩家名称、玩家密码、所得分数、总场次、总分数、赢场次即可
#### 函数设计
- select_by_name: 根据用户名查找用户信息， 用于实现登录功能
- insert: 新增用户，用户实现注册功能
- login: 登录验证，并获取完整的用户信息
- win: 用于给获胜玩家修改分数
- lose: 用户给失败玩家修改分数

### 在线用户管理
对于当前游戏大厅和游戏房间中的用户进行管理，主要是建立起用户与Socket连接
的映射关系，这个模块具有两个功能：
1. 能够让程序中根据用户信息，进而找到能够与用户客户端进行通信的Socket连接，进而实现与客户端的通信。
2. 判断一个用户是否在线，或者判断用户是否已经掉线

### 游戏房间管理模块
设计一个房间类，能够实现房间的实例化，房间类主要是对匹配成对的玩家建立一个小范
围的关联关系，一个房间中任意一个用户发生的任何动作，都会被广播给房间中的其他用户。包括:
1. 棋局对战
2. 实时聊天


### 房间管理类
1. 创建房间，（两个玩家都在线，匹配完成，创建一个新房间，传入玩家ID）
2. 查找房间，（根据用户id可以查找到房间，根据房间id可以查找到房间）
3. 销毁房间，（采用智能指针进行资源释放，当所有用户都退出了，就可以进行房间的销毁）
需要管理的数据有：
- 1.数据管理模块句柄；
- 2.在线用户管理模块句柄；
- 3.房间id分配计数器；
- 4.互斥锁
- 5.两个哈希表，第一个对应用户与房间id的关系，第二个对应房间id与房间指针的关系

### session管理模块
Cookie和Session都是Web开发中保持用户会话状态的技术，核心区别是Cookie存储在客户端，Session存储在服务器端。

#### Cookie：
- 存储位置：用户的浏览器（本地文件），数据以键值对的文本形式存在。
- 核心特点：可设置过期时间（永久/临时），随HTTP请求自动携带，安全性较低（易被篡改或窃取）。
- 常见用途：记住登录状态、保存用户偏好（如主题、语言）、记录购物车临时数据。

#### Session：
- 存储位置：Web服务器（内存、数据库或缓存中），每个用户对应唯一Session ID。
- 核心特点：通过Session ID（通常存于Cookie中）关联用户，数据不暴露给客户端，安全性更高。
- 常见用途：存储用户敏感信息（如登录凭证、权限等级），会话结束或超时后自动销毁。


### session类设计实现
- 一个session类需要使用定时器对每个创建的session对象进行时间管理，如果一个session对象在指定的时间内没有被使用，就会被销毁，如果再次被使用，就会重新设置定时器。
- _ssid使用时间戳填充。实际上， 我们通常使用唯一id生成器生成一个唯一的id
- _user保存当前用户的信息
- timer_ptr _tp保存当前session对应的定时销毁任务(websocket中带有set_timer定时器，直接调用即可)

### session管理设计实现
session的管理主要包含下列几个重要函数：
1. create_session: 创建一个新的session
2. get_session: 通过ssid获取session
3. check_session: 通过ssid判断session是否存在
4. destroy_session: 销毁session
5. set_timer: 为session设置过期时间，过期后session被销毁

### 五子棋对战玩家匹配管理设计实现
匹配队列实现
五子棋对战的玩家匹配是根据自己的天梯分数进行匹配的，而服务器中将玩家天梯分数分为三个档
次：
1. 青铜：天梯分数小于2000分
2. 白银：天梯分数介于2000~3000分之间
3. 黄金：天梯分数大于3000分

### 天梯匹配对战模块：
1. 将所有的玩家分为三个段位：青铜、白银、黄金
2. 每个玩家在匹配时，只能与相同段位的玩家进行匹配
3. 创建三个匹配队列，玩家符合哪个段位就加入到对应的段位匹配队列
4. 创建房间时，检测三个匹配队列，如果有一个队列元素>=2，满足建房条件，此时出队列，向两个玩家发送匹配成功的消息

设计：
1. 设计一个匹配队列——阻塞队列；包含：入队、出队、判断队列是否为空、获取队列大小、等待、唤醒等功能
2. 匹配队列有三个：因此创建三个线程，阻塞等待指定队列中的玩家数量>=2
3. 匹配管理：1）三个不同段位的队列；2）三个线程分别对三个队列中的玩家进行匹配；3）房间管理模块的句柄；4）在线用户管理的句柄
5）数据管理模块的句柄

### 整合实现：
服务端：
1. 网路通信接口的设计
2. 搭建websocket服务器
客户端：
1. 从服务器获取登录页面
2. 给服务器发送登录请求
3. 从服务器获取注册页面
4. 给服务器发送注册请求
5. 从服务器获取游戏大厅页面
6. 从HTTP协议升级到Websocket长连接协议
7. 发送对战匹配请求
8. 发送停止匹配请求
9. 对战匹配成功，建立房间，获取游戏房间页面
10. 发送下棋/聊天请求
11. 对战出结果，返回游戏大厅


### 项目总结：
1. 项目名称为：在线五子棋，实现了五子棋服务器，让用户通过访问服务器，进行用户注册登录、匹配对战、聊天等功能。
2. 开发环境为Ubuntu-24.04/vim/cmake/gdb/vscode
3. 项目大致实现：
- 数据管理模块：基于mysql进行数据管理并实现数据库访问；
- 网络服务模块：基于websocket协议搭建websocket服务器；
- session管理模块：封装session管理，实现http客户端通信状态的维护及身份识别；
- 在线用户管理模块：对于进入大厅/房间的长连接进行管理，随时发送消息至客户端；
- 游戏房间模块：对于同一个房间中的用户动作进行处理；
- 对战匹配管理：创建多线程&阻塞队列，实现同等级分数段位匹配；
- 业务处理：通过发送的不同请求完成相应任务； 