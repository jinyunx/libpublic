#include "HttpDispatch.h"
#include <boost/bind.hpp>
#include <iostream>

#define RSP_OK "{\"code\": 0, \"message\": \"\"}\n"
#define RSP_ERROR "{\"code\": 1, \"message\": \"Bad Method\"}\n"
#define RSP_NOTFOUND "{\"code\": 1, \"message\": \"Not Found\"}\n"

HttpDispatch::HttpDispatch(unsigned short port,
                           boost::asio::io_service service)
    : m_server(port, service)
{ }

void HttpDispatch::Go()
{
    m_server.SetHttpCallback(
        boost::bind(&HttpDispatch::OnRequest, this,
                    _1, _2));
    m_server.Go();
}

void HttpDispatch::AddHandler(const std::string &url,
                             const HttpHandler &handler)
{
    m_handlers[url] = handler;
}

void HttpDispatch::ResponseOk(HttpResponser &resp)
{
    resp.SetStatusCode(HttpResponser::StatusCode_200Ok);
    resp.SetBody(RSP_OK);
}

void HttpDispatch::ResponseError(HttpResponser &resp)
{
    resp.SetStatusCode(HttpResponser::StatusCode_400BadRequest);
    resp.SetBody(RSP_ERROR);
    resp.SetCloseConnection(true);
}

void HttpDispatch::OnRequest(const HttpRequester &req,
                             HttpResponser &resp)
{
    std::cerr << req.GetMethod() << " " << req.GetUrl();
    HanderMap::iterator it = m_handlers.find(req.GetUrl().c_str());
    if (it == m_handlers.end())
    {
        resp.SetStatusCode(HttpResponser::StatusCode_404NotFound);
        resp.SetBody(RSP_NOTFOUND);
        resp.SetCloseConnection(true);
    }
    else
    {
        it->second(req, resp);
    }
}

