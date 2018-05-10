#include "HttpServer.h"
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/enable_shared_from_this.hpp>

class Session : public boost::enable_shared_from_this<Session>,
                private boost::noncopyable
{
public:

    explicit Session(boost::asio::io_service service)
        : m_service(service),
          m_socket(m_service),
          m_timer(m_service)
    { }

    boost::asio::ip::tcp::socket &Socket()
    {
        return m_socket;
    }

    void SetHttpCallback(const HttpCallback &httpCallback)
    {
        m_httpCallback = httpCallback;
    }

    void Go()
    {
        boost::asio::spawn(
            m_service, boost::bind(&Session::HttpService,
                                   shared_from_this(), _1));
        boost::asio::spawn(
            m_service, boost::bind(&Session::Timeout,
                                   shared_from_this(), _1));
    }

private:
    bool RefreshTimer()
    {
        boost::system::error_code error;
        m_timer.expires_from_now(
            boost::posix_time::seconds(kTimeout), error);

        return !error;
    }

    bool AppendData(std::vector<char> &data,
                    boost::asio::yield_context yield)
    {
        char buffer[kBufferSize];

        boost::system::error_code error;
        std::size_t size = m_socket.async_read_some(
            boost::asio::buffer(buffer), yield[error]);

        if (error)
            return false;

        data.insert(data.end(), buffer, buffer + size);
        return true;
    }

    void Shutdown(const std::string &response,
                  boost::asio::yield_context yield)
    {
        if (!response.empty())
        {
            boost::system::error_code ignoreError;
            boost::asio::async_write(
                m_socket, response, yield[ignoreError]);
        }
        boost::system::error_code ignoreError;
        m_socket.close(ignoreError);
        m_timer.cancel(ignoreError);
    }

    bool WriteResponse(HttpResponser &responser,
                       boost::asio::yield_context yield)
    {
        std::string buffer;
        responser.AppendToBuffer(buffer);

        boost::system::error_code error;
        boost::asio::async_write(
            m_socket, boost::asio::buffer(buffer), yield[error]);

        return !error;
    }

    void HttpService(boost::asio::yield_context yield)
    {
        std::vector<char> data;
        HttpRequester httpRequester;

        while (1)
        {
            if (!RefreshTimer())
            {
                Shutdown("HTTP/1.1 500 Http server"
                         " timer error\r\n\r\n", yield);
            }

            if (AppendData(data, yield) &&
                httpRequester.Parse(&data[0], data.size()))
            {
                if (httpRequester.IsComplete())
                {
                    if (!OnRequest(httpRequester, yield))
                        return;

                    httpRequester.Reset();
                    data.resize(0);
                }
            }
            else
            {
                Shutdown("HTTP/1.1 400 Http recv or"
                         " parse error\r\n\r\n", yield);
            }
        }
    }

    // Return:
    //    true: keep alive
    //    false: has been shutdown
    bool OnRequest(HttpRequester &requester,
                   boost::asio::yield_context yield)
    {
        if (m_httpCallback)
        {
            bool close = requester.GetHeader("Connection") == "close";

            HttpResponser responser(close);
            m_httpCallback(requester, responser);

            if (!WriteResponse(responser, yield) ||
                responser.CloseConnection())
            {
                Shutdown("", yield);
                return false;
            }
            return true;
        }
        else
        {
            Shutdown("HTTP/1.1 404 Not Found\r\n\r\n", yield);
            return false;
        }
    }

    void Timeout(boost::asio::yield_context yield)
    {
        while (m_socket.is_open())
        {
            boost::system::error_code error;
            m_timer.async_wait(yield[error]);
            if (error != boost::asio::error::operation_aborted)
            {
                boost::system::error_code ignoreError;
                m_socket.close(ignoreError);
            }
        }
    }

    const static int kBufferSize = 128;
    const static int kTimeout = 10;

    boost::asio::io_service &m_service;
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::deadline_timer m_timer;

    HttpCallback m_httpCallback;
};


HttpServer::HttpServer(unsigned short port,
                       boost::asio::io_service service)
    : m_port(port), m_service(service)
{ }

void HttpServer::SetHttpCallback(const HttpCallback &httpCallback)
{
    m_httpCallback = httpCallback;
}

void HttpServer::Go()
{
    boost::asio::spawn(
        m_service, boost::bind(&HttpServer::DoAccept, this, _1));
}

void HttpServer::DoAccept(boost::asio::yield_context yield)
{
    boost::asio::ip::tcp::acceptor acceptor(
        m_service, boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(), m_port));

    while (1)
    {
        boost::system::error_code error;
        boost::shared_ptr<Session> session(new Session(m_service));
        acceptor.async_accept(session->Socket(), yield[error]);
        if (!error)
            session->Go();
    }
}
