#include "HttpServer.h"

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <iostream>

class HttpSession : public Session
{
public:
    explicit HttpSession(boost::asio::io_service &service,
                         const HttpCallback &httpCallback)
        : Session(service),
          m_httpCallback(httpCallback)
    { }

private:
    virtual bool OnData(const char *buffer, std::size_t bufferLength)
    {
        if (!m_httpRequester.Parse(buffer, bufferLength))
            return false;

        if (m_httpRequester.IsComplete())
        {
            if (!OnRequest())
                return false;
            m_httpRequester.Reset();
        }

        return true;
    }

    virtual void OnClose()
    { }

    bool OnRequest()
    {
        bool close = m_httpRequester.GetHeader("Connection") ==
            std::string("close");

        HttpResponser responser(close);

        if (m_httpCallback)
        {
            m_httpCallback(m_httpRequester, responser);
        }
        else
        {
            responser.SetStatusCode(HttpResponser::StatusCode_404NotFound);
            responser.SetBody("{\"code\": -1, \"message\": \"Not Found\"}\n");
            responser.SetCloseConnection(true);
        }

        std::string buff;
        responser.AppendToBuffer(buff);

        WriteResponse(buff.data(), buff.size());
        return !responser.CloseConnection();
    }

    HttpRequester m_httpRequester;
    HttpCallback m_httpCallback;
};

HttpServer::HttpServer(unsigned short port,
                       boost::asio::io_service &service,
                       const HttpCallback &httpCallback)
    : m_service(service),
      m_httpCallback(httpCallback),
      m_tcpServer(port, service, boost::bind(
          &HttpServer::NewSession, this))
{ }

void HttpServer::Go()
{
    m_tcpServer.Go();
}

SessionPtr HttpServer::NewSession()
{
    return SessionPtr(new HttpSession(m_service, m_httpCallback));
}
