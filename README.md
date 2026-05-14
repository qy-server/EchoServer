  EchoServer 采用主从 Reactor 模型。主 EventLoop 只负责监听服务端 socket 并接收新连接；TcpServer 将新连接按照 fd % threadnum 分发到多个 Sub EventLoop，每个 Sub EventLoop 独立运行在 IO 线程池中，通过 epoll 监听客户端读写事件。Connection 封装客户端连接，Channel 封装 fd 事件与回调，Buffer 负责收发缓冲和报文拆包。业务层 EchoServer 注册连接、关闭、错误、消息、发送完成等回调，收到消息后可投递到 WORKS 业务线程池处理，处理完成后通过 conn->send() 将响应交回对应 IO 线程发送。EventLoop 内部使用 eventfd 实现跨线程唤醒，使用 timerfd 定时清理超时连接。
```mermaid
flowchart TB
    Client["Client 客户端"] -->|"TCP connect / request"| Acceptor

    subgraph App["EchoServer 应用层"]
        Main["echoserver.cpp<br/>创建 EchoServer(ip, port, 3, 2)"]
        Echo["EchoServer<br/>注册业务回调"]
        WorkPool["WORKS ThreadPool<br/>业务线程池"]
        Biz["OnMessage<br/>reply: message"]
    end

    subgraph Tcp["TcpServer 网络层"]
        TcpServer["TcpServer<br/>管理连接与回调"]
        Acceptor["Acceptor<br/>监听 socket / accept 新连接"]
        ConnMap["conns_<br/>fd -> Connection"]
    end

    subgraph Reactor["Reactor 事件循环层"]
        MainLoop["Main EventLoop<br/>负责监听连接"]
        IOPool["IO ThreadPool"]
        SubLoop1["Sub EventLoop 1"]
        SubLoop2["Sub EventLoop 2"]
        SubLoopN["Sub EventLoop N"]
        Epoll["Epoll<br/>epoll_create / epoll_wait / epoll_ctl"]
        Channel["Channel<br/>封装 fd 和事件回调"]
    end

    subgraph Conn["连接与缓冲层"]
        Connection["Connection<br/>读写客户端 socket"]
        InBuf["input Buffer<br/>拆包"]
        OutBuf["output Buffer<br/>发送缓冲"]
        Socket["Socket / InetAddress<br/>封装 fd、ip、port"]
    end

    subgraph Timer["辅助机制"]
        EventFd["eventfd<br/>跨线程唤醒 EventLoop"]
        TimerFd["timerfd<br/>定时检测超时连接"]
    end

    Main --> Echo
    Echo --> TcpServer
    Echo --> WorkPool

    TcpServer --> MainLoop
    TcpServer --> Acceptor
    TcpServer --> IOPool
    IOPool --> SubLoop1
    IOPool --> SubLoop2
    IOPool --> SubLoopN

    MainLoop --> Epoll
    SubLoop1 --> Epoll
    SubLoop2 --> Epoll
    SubLoopN --> Epoll

    Acceptor -->|"accept 后回调 TcpServer::newconnection"| TcpServer
    TcpServer -->|"fd % threadnum 分发"| Connection
    TcpServer --> ConnMap
    Connection --> Channel
    Channel --> Epoll
    Connection --> InBuf
    Connection --> OutBuf
    Connection --> Socket

    Channel -->|"EPOLLIN"| Connection
    Connection -->|"onmessage callback"| TcpServer
    TcpServer -->|"EchoServer::HandleMessage"| Echo
    Echo -->|"addtask"| WorkPool
    WorkPool --> Biz
    Biz -->|"conn->send()"| Connection
    Connection -->|"非 IO 线程则 queueinloop"| EventFd
    EventFd --> SubLoop1
    Connection -->|"EPOLLOUT"| OutBuf

    TimerFd --> SubLoop1
    TimerFd --> SubLoop2
    TimerFd --> SubLoopN
    TimerFd -->|"清理超时连接"| ConnMap

```
