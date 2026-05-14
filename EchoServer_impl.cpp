#include "EchoServer.h"

EchoServer::EchoServer(const std::string &ip,const uint16_t port,int subthreadnum,int workthreadnum)
                   :tcpserver_(ip,port,subthreadnum),threadpool_(workthreadnum,"WORKS")
{
    tcpserver_.setnewconnectioncb(std::bind(&EchoServer::HandleNewConnection, this, std::placeholders::_1));
    tcpserver_.setcloseconnectioncb(std::bind(&EchoServer::HandleClose, this, std::placeholders::_1));
    tcpserver_.seterrorconnectioncb(std::bind(&EchoServer::HandleError, this, std::placeholders::_1));
    tcpserver_.setonmessagecb(std::bind(&EchoServer::HandleMessage, this, std::placeholders::_1, std::placeholders::_2));
    tcpserver_.setsendcompletecb(std::bind(&EchoServer::HandleSendComplete, this, std::placeholders::_1));
}

EchoServer::~EchoServer()
{
}

void EchoServer::Start()
{
    tcpserver_.start();
}

void EchoServer::Stop()
{
    threadpool_.stop();
    printf("work thread pool stopped.\n");
    tcpserver_.stop();
}

void EchoServer::HandleNewConnection(spConnection conn)
{
    printf("new connection(fd=%d,ip=%s,port=%d) ok.\n",conn->fd(),conn->ip().c_str(),conn->port());
}

void EchoServer::HandleClose(spConnection conn)
{
    printf("connection closed.(fd=%d,ip=%s,port=%d)\n",conn->fd(),conn->ip().c_str(),conn->port());
}

void EchoServer::HandleError(spConnection conn)
{
    printf("connection error.(fd=%d,ip=%s,port=%d)\n",conn->fd(),conn->ip().c_str(),conn->port());
}

void EchoServer::HandleMessage(spConnection conn,std::string& message)
{
    if (threadpool_.size() == 0)
    {
        OnMessage(conn,message);
    }
    else
    {
        threadpool_.addtask(std::bind(&EchoServer::OnMessage,this,conn,message));
    }
}

void EchoServer::OnMessage(spConnection conn,std::string& message)
{
    printf("%s message(fd=%d):%s\n",Timestamp::now().tostring().c_str(),conn->fd(),message.c_str());
    message="reply:"+message;
    conn->send(message.data(),message.size());
}

void EchoServer::HandleSendComplete(spConnection conn)
{
    printf("send complete.(fd=%d,ip=%s,port=%d)\n",conn->fd(),conn->ip().c_str(),conn->port());
}
