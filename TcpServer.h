#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <iostream>

class Session : public boost::enable_shared_from_this<Session>,
                private boost::noncopyable
{
public:
    explicit Session(boost::asio::io_service &service)
        : m_service(service),
          m_socket(m_service)
    { }

    virtual ~Session()
    {
        Shutdown();
    }

    boost::asio::ip::tcp::socket & Socket()
    {
        return m_socket;
    }

    std::string GetRemoteIpString() const
    {
        return m_remoteIpString;
    }

    void Go()
    {
        CacheRemoteIpString();
        boost::asio::spawn(
            m_service, boost::bind(&Session::TcpService,
                                   shared_from_this(), _1));
    }

protected:
    virtual bool OnData(const char *buffer, std::size_t bufferLength) = 0;
    virtual void OnClose() = 0;

    void TcpService(boost::asio::yield_context yield)
    {
        char buffer[kBufferSize];
        while (1)
        {
            std::size_t bufferLength = kBufferSize;
            if (!ReadData(buffer, &bufferLength, yield))
                break;

            if (!OnData(buffer, bufferLength))
                break;
        }
        Shutdown();
    }

    bool ReadData(char *buffer, std::size_t *len,
                  boost::asio::yield_context yield)
    {
        boost::asio::deadline_timer timer(m_service);
        StartTimer(timer, kTimeout);

        boost::system::error_code error;
        std::size_t size = m_socket.async_read_some(
            boost::asio::buffer(buffer, *len), yield[error]);

        CancelTimer(timer);

        *len = size;

        return !error;
    }

    void WriteResponse(const char *buffer, std::size_t len)
    {
        boost::asio::spawn(
            m_service, boost::bind(&Session::WriteData, shared_from_this(),
                                   buffer, len, _1));
    }

    bool WriteData(const char *buffer, std::size_t len,
                   boost::asio::yield_context yield)
    {
        boost::asio::deadline_timer timer(m_service);
        StartTimer(timer, kTimeout);

        std::string tmpBuf(buffer, len);
        boost::system::error_code error;
        boost::asio::async_write(
            m_socket, boost::asio::buffer(tmpBuf.data(), tmpBuf.size()), yield[error]);

        CancelTimer(timer);

        return !error;
    }

    void Shutdown()
    {
        if (!m_socket.is_open())
            return;

        OnClose();
        boost::system::error_code ignoreError;
        m_socket.close(ignoreError);
    }

    void StartTimer(boost::asio::deadline_timer &timer, int timeToTimeout)
    {
        boost::system::error_code ignoreError;
        timer.expires_from_now(
            boost::posix_time::seconds(timeToTimeout), ignoreError);

        boost::asio::spawn(
            m_service, boost::bind(&Session::Timer, shared_from_this(),
                                   boost::ref(timer), _1));
    }

    void Timer(boost::asio::deadline_timer &timer, boost::asio::yield_context yield)
    {
        boost::system::error_code error;
        timer.async_wait(yield[error]);
        if (error != boost::asio::error::operation_aborted)
        {
            std::cout << "Timeout" << std::endl;
            Shutdown();
        }
    }

    void CancelTimer(boost::asio::deadline_timer &timer)
    {
        boost::system::error_code ignoreError;
        timer.cancel(ignoreError);
    }

    void CacheRemoteIpString()
    {
        boost::system::error_code err;
        boost::asio::ip::tcp::endpoint endpoint = m_socket.remote_endpoint(err);

        if (!err)
            m_remoteIpString = endpoint.address().to_string();
    }

    const static int kBufferSize = 128;
    const static int kTimeout = 10;

    boost::asio::io_service &m_service;
    boost::asio::ip::tcp::socket m_socket;
    std::string m_remoteIpString;
};

typedef boost::shared_ptr<Session> SessionPtr;

class TcpServer
{
public:
    typedef boost::function< SessionPtr() > NewSession;

    TcpServer(unsigned short port,
             boost::asio::io_service &service,
             const NewSession &newSession)
        : m_port(port), m_service(service),
          m_newSession(newSession)
    { }

    void Go()
    {
        boost::asio::spawn(
            m_service, boost::bind(&TcpServer::DoAccept,
                                   this, _1));
    }

private:
    void DoAccept(boost::asio::yield_context yield)
    {
        boost::asio::ip::tcp::acceptor acceptor(
            m_service, boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(), m_port));

        while (1)
        {
            boost::system::error_code error;
            SessionPtr session = m_newSession();
            acceptor.async_accept(session->Socket(), yield[error]);
            if (!error)
                session->Go();
        }
    }

    unsigned short m_port;
    boost::asio::io_service &m_service;
    NewSession m_newSession;
};

#endif // TCP_SERVER_H
