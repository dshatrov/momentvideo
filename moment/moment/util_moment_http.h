#ifndef MOMENT__UTIL_MOMENT_HTTP__H__
#define MOMENT__UTIL_MOMENT_HTTP__H__


#define MOMENT_SERVER__HEADERS_DATE \
        Byte date_buf [unixtimeToString_BufSize]; \
        Size const date_len = unixtimeToString (Memory::forObject (date_buf), getUnixtime());

#define MOMENT_SERVER__COMMON_HEADERS \
        "Server: Moment/1.0\r\n" \
        "Date: ", ConstMemory (date_buf, date_len), "\r\n" \
        "Connection: Keep-Alive\r\n"

#define MOMENT_SERVER__HEADERS(status, mime_type, content_length) \
        "HTTP/1.1 ", status, "\r\n" \
        MOMENT_SERVER__COMMON_HEADERS \
        "Content-Type: ", (mime_type), "\r\n" \
        "Content-Length: ", (content_length), "\r\n" \
        "Cache-Control: no-cache\r\n"

#define MOMENT_SERVER__OK_HEADERS(mime_type, content_length) \
        MOMENT_SERVER__HEADERS ("200 OK", mime_type, content_length)

#define MOMENT_SERVER__400_HEADERS(content_length) \
        MOMENT_SERVER__HEADERS ("400 Bad Request", "text/plain", content_length)

#define MOMENT_SERVER__404_HEADERS(content_length) \
        MOMENT_SERVER__HEADERS ("404 Not Found", "text/plain", content_length)

#define MOMENT_SERVER__500_HEADERS(content_length) \
        MOMENT_SERVER__HEADERS ("500 Internal Server Error", "text/plain", content_length)


#endif /* MOMENT__UTIL_MOMENT_HTTP__H__ */

