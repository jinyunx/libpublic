#include "HttpParser.h"
#include <sstream>

http_parser_settings HttpParser::m_settings =
{
    0,
    &HttpParser::OnUrl,
    0,
    &HttpParser::OnHeaderName,
    &HttpParser::OnHeaderValue,
    0,
    &HttpParser::OnBody,
    &HttpParser::OnComplete,
    0,
    0
};

HttpParser::HttpParser()
    : m_headerState(HeaderState_None),
      m_complete(false)
{
    http_parser_init(&m_httpParser, HTTP_BOTH);
    m_httpParser.data = this;
}

bool HttpParser::Parse(const char *data, size_t len)
{
    http_parser_execute(&m_httpParser, &m_settings, data, len);
    return HTTP_PARSER_ERRNO(&m_httpParser) == HPE_OK;
}

bool HttpParser::IsComplete() const
{
    return m_complete;
}

void HttpParser::Reset()
{
    http_parser_init(&m_httpParser, HTTP_BOTH);
    m_httpParser.data = this;
    m_complete = false;
    m_headerState = HeaderState_None;
    m_url.clear();
    m_method.clear();
    m_headerName.clear();
    m_headerValue.clear();
    m_headers.clear();
    m_body.clear();
}

const char * HttpParser::GetErrorDetail() const
{
    return http_errno_description(HTTP_PARSER_ERRNO(&m_httpParser));
}

const std::string & HttpParser::GetUrl() const
{
    return m_url;
}

const std::string & HttpParser::GetMethod() const
{
    return m_method;
}

const std::string & HttpParser::GetBody() const
{
    return m_body;
}

const HttpParser::HeaderMap & HttpParser::GetHeaders() const
{
    return m_headers;
}

const char * HttpParser::GetHeader(const std::string &name) const
{
    HeaderMap::const_iterator it = m_headers.find(name);
    if (it != m_headers.end())
        return it->second.c_str();
    return "";
}

std::string HttpParser::ToString() const
{
    std::ostringstream oss;
    oss << "method: " << GetMethod()
        << " url: " << GetUrl()
        << "\nbody:\n" << GetBody()
        << "\n";

    return oss.str();
}

bool HttpParser::HeaderReady() const
{
    return m_headerState == HeaderState_HasAll;
}

int HttpParser::OnUrl(http_parser * parser, const char * at, size_t length)
{
    if (length <= 0)
        return 0;

    HttpParser *httpParser = (HttpParser *)(parser->data);
    httpParser->m_url.append(at, length);
    httpParser->m_method = http_method_str(
        static_cast<enum http_method>(parser->method));

    return 0;
}

int HttpParser::OnHeaderName(http_parser * parser, const char * at, size_t length)
{
    if (length <= 0)
        return 0;

    HttpParser *httpParser = (HttpParser *)(parser->data);

    httpParser->m_headerName = "";
    httpParser->m_headerName.append(at, length);
    httpParser->m_headerState =
        HeaderState(httpParser->m_headerState | HeaderState_HasName);
    if (httpParser->m_headerState == HeaderState_HasAll)
    {
        httpParser->m_headerState = HeaderState_None;
        httpParser->m_headers[httpParser->m_headerName] = httpParser->m_headerValue;
    }

    return 0;
}

int HttpParser::OnHeaderValue(http_parser * parser, const char * at, size_t length)
{
    if (length <= 0)
        return 0;

    HttpParser *httpParser = (HttpParser *)(parser->data);

    httpParser->m_headerValue = "";
    httpParser->m_headerValue.append(at, length);
    httpParser->m_headerState =
        HeaderState(httpParser->m_headerState | HeaderState_HasVaule);
    if (httpParser->m_headerState == HeaderState_HasAll)
    {
        httpParser->m_headerState = HeaderState_None;
        httpParser->m_headers[httpParser->m_headerName] =
            httpParser->m_headerValue;
    }

    return 0;
}

int HttpParser::OnBody(http_parser * parser, const char * at, size_t length)
{
    if (length <= 0)
        return 0;

    HttpParser *httpParser = (HttpParser *)(parser->data);
    httpParser->m_body.append(at, length);

    return 0;
}

int HttpParser::OnComplete(http_parser * parser)
{
    HttpParser *httpParser = (HttpParser *)(parser->data);
    httpParser->m_complete = true;
    return 0;
}
