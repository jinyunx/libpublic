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
#include <deque>

namespace
{
    void PrintBuf(char *buf, size_t size)
    {
        for (size_t i = 0; i < size; ++i)
        {
            fprintf(stderr, "%x ", buf[i] & 0xff);
            if ((i + 1) % 16 == 0)
                fprintf(stderr, "\n");
        }
        fprintf(stderr, "\n");
    }
}

typedef std::string Buffer;
typedef boost::shared_ptr<Buffer> BufferPtr;
typedef std::deque<BufferPtr> DequeBuffer;

class Session : public boost::enable_shared_from_this<Session>,
                private boost::noncopyable
{
public:
    explicit Session(boost::asio::io_service &service)
        : m_writeTimeout(kTimeout),
          m_readTimeout(kTimeout),
          m_writing(false),
          m_service(service),
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

    void SetWriteTimeout(unsigned int t)
    {
        m_writeTimeout = t;
    }

    void SetReadTimeout(unsigned int t)
    {
        m_readTimeout = t;
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
            {
                std::cerr << "read data error" << std::endl;
                break;
            }

            if (!OnData(buffer, bufferLength))
            {
                std::cerr << "data process error" << std::endl;
                break;
            }
        }
        Shutdown();
    }

    bool ReadData(char *buffer, std::size_t *len,
                  boost::asio::yield_context yield)
    {
        boost::asio::deadline_timer timer(m_service);
        StartTimer(timer, m_readTimeout);

        boost::system::error_code error;
        std::size_t size = m_socket.async_read_some(
            boost::asio::buffer(buffer, *len), yield[error]);

        CancelTimer(timer);

        *len = size;

        return !error;
    }

    void WriteResponse(const char *buffer, std::size_t len)
    {
        BufferPtr data(new Buffer(buffer, len));
        WriteResponse(data);
    }

    void WriteResponse(const BufferPtr &data)
    {
        m_writeBuffer.push_back(data);
        if (!m_writing)
        {
            m_writing = true;
            boost::asio::spawn(
                m_service, boost::bind(&Session::WriteData, shared_from_this(),
                                       _1));
        }
    }

    bool WriteData(boost::asio::yield_context yield)
    {
        boost::system::error_code error;
        while (!error && !m_writeBuffer.empty())
        {
            boost::asio::deadline_timer timer(m_service);
            StartTimer(timer, m_writeTimeout);

            BufferPtr buffer = m_writeBuffer.front();
            m_writeBuffer.pop_front();

            boost::asio::async_write(
                m_socket, boost::asio::buffer(buffer->data(), buffer->size()), yield[error]);

            CancelTimer(timer);
        }

        m_writing = false;
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
        if (!timeToTimeout)
            return;

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

    unsigned int m_writeTimeout;
    unsigned int m_readTimeout;

    bool m_writing;
    DequeBuffer m_writeBuffer;

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
