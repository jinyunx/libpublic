#include "HttpDispatch.h"

void Handler(const HttpRequester & req,
             HttpResponser &resp)
{
    resp.SetBody("hello");
}

int main()
{
    boost::asio::io_service service;

    HttpDispatch http(7070, service);
    http.AddHandler("/", Handler);
    http.Go();
    return 0;
}
