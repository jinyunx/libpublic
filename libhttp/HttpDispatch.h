#ifndef HTTP_DISPATCH_H
#define HTTP_DISPATCH_H

#include "HttpServer.h"
#include <boost/function.hpp>
#include <string>
#include <map>

class HttpDispatch : private boost::noncopyable
{
public:
    typedef boost::function< void(const HttpRequester &,
                             HttpResponser &) > HttpHandler;

    HttpDispatch(unsigned short port,
                 boost::asio::io_service &service);

    void Go();
    void AddHandler(const std::string &url, const HttpHandler &handler);
    void ResponseOk(HttpResponser &resp);
    void ResponseError(HttpResponser &resp);

private:
    typedef std::map<std::string, HttpHandler> HanderMap;

    void OnRequest(const HttpRequester &req,
                   HttpResponser &resp);

    HanderMap m_handlers;
    HttpServer m_server;
};

#endif // HTTP_DISPATCH_H
