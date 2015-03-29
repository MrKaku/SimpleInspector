#ifndef SIMPLEINSPECTOR_H
#define SIMPLEINSPECTOR_H

#include <string>
#include <map>
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>


//该类会启动一个线程，并在线程中监听端口，响应连接
//典型应用是使用浏览器连接到指定的端口，该类会根据输入的url，返回不同的结果，
//返回结果由类的使用者自定义，一般是根据url返回进程内部的某些状态信息。

class SimpleInspector
{
private:
    typedef boost::function<std::string()> RequestHandler;
    typedef std::map<std::string, RequestHandler> HandlerMap;

public:
    SimpleInspector(int port);
    ~SimpleInspector();

	//增加Url和对应的函数对象
    void AddInspectee(const std::string& sUrl, const RequestHandler& handler);
	
	//启动一个线程，在该线程中监听端口m_port，如果有连接
    bool RunInThread();
	
	//退出Inspector线程
    bool Stop();

private:
    void Run();
    void HandleConn(int sockFd);
    std::string HandleRequest(const std::string& sUrl);
    static void* InspectorThread(void *pArg);

private:
    int           m_port;				//监听的端口号
    int           m_stopFlag;			//停止标志
    pthread_t     m_tid;				//Inspector所在线程id

    HandlerMap    m_handlers;			//Url对应的处理函数对象表
    boost::mutex  m_handlersMutex;		//对m_handlers的保护
};

#endif // SIMPLEINSPECTOR_H
