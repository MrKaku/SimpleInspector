#include "simpleinspector.h"

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <strings.h>
#include <errno.h>
#include <string>
#include <pthread.h>

using namespace std;


namespace detail
{
int CreateAndListen(int port)
{
    int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(listenfd != -1);

    sockaddr_in localAddr;
    bzero(&localAddr, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(port);
    int ret = bind(listenfd, (sockaddr*)&localAddr, sizeof(localAddr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    return listenfd;
}

bool ParseRequest(const string& sRequestLine, string& sMethod, string& sUrl)
{
    //get method
    size_t iFirstSpace = sRequestLine.find(" ");
    if(iFirstSpace == string::npos)
    {
        return false;
    }
    sMethod = sRequestLine.substr(0, iFirstSpace);

    //get url
    size_t iSecondSpace = sRequestLine.find(" ", iFirstSpace + 1);
    if(iSecondSpace == string::npos)
    {
        return false;
    }
    sUrl = sRequestLine.substr(iFirstSpace + 1, iSecondSpace - iFirstSpace - 1);

    return true;
}
}


SimpleInspector::SimpleInspector(int port)
  : m_port(port)
  , m_stopFlag(0)
  , m_tid(0)
{
}

SimpleInspector::~SimpleInspector()
{
    if(__sync_fetch_and_add(&m_stopFlag, 0) == 0)
    {
        Stop();
    }
}

void SimpleInspector::AddInspectee(const string& sUrl, const RequestHandler& handler)
{
    boost::lock_guard<boost::mutex> guard(m_handlersMutex);
    (void)guard;

    m_handlers[sUrl] = handler;
}

void* SimpleInspector::InspectorThread(void *pArg)
{
    if(!pArg)
    {
        return NULL;
    }

    SimpleInspector* pInspector = static_cast<SimpleInspector*>(pArg);
    pInspector->Run();

    return NULL;
}

bool SimpleInspector::RunInThread()
{
    if(m_tid != 0)
    {
        return true;
    }

    return 0 == pthread_create(&m_tid, NULL, InspectorThread, this);
}

bool SimpleInspector::Stop()
{
    __sync_fetch_and_add(&m_stopFlag, 1);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        printf("create socket failed, errno:%d\n", errno);
        return false;
    }

    sockaddr_in localAddr;
    bzero(&localAddr, sizeof(localAddr));
    localAddr.sin_family = PF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(m_port);
    int ret = connect(sock, (sockaddr*)&localAddr, sizeof(localAddr));
    if(ret < 0)
    {
        printf("connect failed, errno:%d\n", errno);
        return false;
    }

    close(sock);

    return true;
}

void SimpleInspector::Run()
{
    int listenFd = detail::CreateAndListen(m_port);

    while(1)
    {
        sockaddr_in remote;
        socklen_t len = sizeof(remote);
        int connFd = accept(listenFd, (sockaddr*)&remote, &len);
        if(__sync_fetch_and_add(&m_stopFlag, 0))
        {
            close(connFd);
            break;
        }

        HandleConn(connFd);
    }
}


void SimpleInspector::HandleConn(int connFd)
{
    string sRequestLine;

    string sRequest;
    char buf[512] = {0};
	
	//tofix.此处假定肯定会有数据过来，
	//如果一直没有数据则Inspector线程会卡死
    while(read(connFd, buf, sizeof buf) > 0) 
    {
        sRequest += buf;
        size_t pos = sRequest.find("\r\n");
        if(pos != string::npos)
        {
            sRequestLine = sRequest.substr(0, pos);
            break;
        }
    }

    string sMethod, sUrl;
    if(!detail::ParseRequest(sRequestLine, sMethod, sUrl))
    {
        close(connFd);
        return;
    }

    string sResponse;
    if(strcasecmp(sMethod.c_str(), "GET") != 0)
    {
        sResponse = "HTTP/1.0 501 Method Not Implemented\r\n";
        //header and body
    }
    else
    {
        sResponse =  "HTTP/1.0 200 OK\r\n";
        sResponse += "Connection: close\r\n";
        sResponse += "\r\n";
        sResponse += HandleRequest(sUrl);
    }

    send(connFd, sResponse.c_str(), sResponse.size(), 0);

    close(connFd);
}

string SimpleInspector::HandleRequest(const std::string &sUrl)
{
    RequestHandler handler;
    {
        boost::lock_guard<boost::mutex> guard(m_handlersMutex);
        (void)guard;

        HandlerMap::iterator it = m_handlers.find(sUrl);
        if(it == m_handlers.end())
        {
            return string("invalid cmd:  ") + sUrl;
        }

        handler = it->second;
    }

    if(handler)
    {
        return handler();
    }
    else
    {
        return "cmd not implemented!";
    }
}

