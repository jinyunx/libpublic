#ifndef HTTP_SERVER
#define HTTP_SERVER

#include "HttpRequester.h"
#include "HttpResponser.h"

#include <boost/asio.hpp>
#include <boost/function.hpp>

#include <stdio.h>

typedef boost::function<
    void(const HttpRequester &, HttpResponser &) > HttpCallback;

class HttpServer
{
public:
    HttpServer(unsigned short port,
               boost::asio::io_service service);
    void SetHttpCallback(const HttpCallback &httpCallback);
    void Go();

private:
    void DoAccept(boost::asio::yield_context yield);

    unsigned short m_port;
    boost::asio::io_service &m_service;
    HttpCallback m_httpCallback;
};

#endif
