#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "HttpRequester.h"
#include "HttpResponser.h"
#include "../TcpServer.h"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/function.hpp>

#include <stdio.h>

typedef boost::function<
    void(const HttpRequester &, HttpResponser &) > HttpCallback;

class HttpServer : private boost::noncopyable
{
public:
    HttpServer(unsigned short port,
               boost::asio::io_service &service,
               const HttpCallback &httpCallback);
    void Go();

private:
    SessionPtr NewSession();

    boost::asio::io_service &m_service;
    HttpCallback m_httpCallback;
    TcpServer m_tcpServer;
};

#endif // HTTP_SERVER_H
