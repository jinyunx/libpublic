#include "HttpDispatch.h"
#include <iostream>

void Handler(const HttpRequester & req,
             HttpResponser &resp)
{
    std::cerr << "method: " << req.GetMethod() << std::endl;

    resp.SetStatusCode(HttpResponser::StatusCode_200Ok);
    resp.SetBody("hello");
}

int main()
{
    boost::asio::io_service service;

    HttpDispatch http(7070, service);
    http.AddHandler("/", Handler);
    http.Go();
    service.run();
    return 0;
}
