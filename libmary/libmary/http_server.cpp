/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>
#include <libmary/util_str.h>
#include <libmary/util_dev.h>

#include <libmary/http_parser.h>

#include <libmary/http_server.h>


namespace M {

static LogGroup libMary_logGroup_http ("http", LogLevel::I);

Receiver::Frontend const HttpServer::receiver_frontend = {
    processInput,
    processEof,
    processError,
    NULL /* processInputFrom */
};

Sender::Frontend const HttpServer::sender_frontend = {
    sendStateChanged,
    senderClosed
};

static bool isHttp_Separator (Byte const byte)
{
    return byte == '(' || byte == ')' || byte == '<' || byte == '>'  || byte == '@' ||
           byte == ',' || byte == ';' || byte == ':' || byte == '\\' || byte == '"' ||
           byte == '/' || byte == '[' || byte == ']' || byte == '?'  || byte == '=' ||
           byte == '{' || byte == '}' ||
           byte == 32 /* SP */ ||
           byte ==  9 /* HT */;
}

static inline bool isHttp_CTL (Byte const byte)
{
    return (byte < 31 || byte == 127);
}

static void skipToken (ConstMemory const mem,
                       Size * const mt_nonnull ret_pos)
{
    Size pos = *ret_pos;
    for (;;) {
        if (pos >= mem.len())
            break;

        Byte const byte = mem.buf() [pos];
        if (isHttp_CTL (byte) || isHttp_Separator (byte))
            break;

        ++pos;
    }

    *ret_pos = pos;
}

static Memory stripLeadTrailWhsp (Memory mem)
{
    {
        Size offs = 0;
        for (; offs < mem.len(); ++offs) {
            if (   mem.buf() [offs] != 32 /* SP */
                && mem.buf() [offs] !=  9 /* HT */
                && mem.buf() [offs] != 13 /* CR */
                && mem.buf() [offs] != 10 /* LF */)
            {
                break;
            }
        }
        mem = mem.region (offs);
    }

    {
        Size offs = mem.len();
        for (; offs > 0; --offs) {
            if (   mem.buf() [offs - 1] != 32 /* SP */
                && mem.buf() [offs - 1] !=  9 /* HT */
                && mem.buf() [offs - 1] != 13 /* CR */
                && mem.buf() [offs - 1] != 10 /* LF */)
            {
                break;
            }
        }
        mem = mem.region (0, offs);
    }

    return mem;
}

mt_sync_domain (receiver_frontend) Result
HttpServer::processRequestLine (Memory  _mem,
                                bool   * const ret_empty_request_line)
{
    logD (http, _this_func, "mem: ", _mem);

    *ret_empty_request_line = false;

    _mem = stripLeadTrailWhsp (_mem);
    if (_mem.len() == 0) {
        *ret_empty_request_line = true;
        return Result::Success;
    }

    cur_req->request_line = st_grab (new (std::nothrow) String (_mem));
    // We want to operate with stored memory in cur_req->request_line,
    // not with temporal _mem.
    Memory const mem = cur_req->request_line->mem();

    Byte const *method_end = NULL;
    Byte const *path_beg   = NULL;
    Byte const *uri_end    = NULL;
    Byte       *proto_beg  = NULL;
    Byte const *proto_end  = NULL;
    {
        Size offs = 0;

        for (; offs < mem.len(); ++offs) {
            if (   mem.buf() [offs] == 32 /* SP */
                || mem.buf() [offs] ==  9 /* HT */)
            {
                break;
            }
        }
        if (offs == mem.len()) {
            if (logLevelOn (http, LogLevel::Debug)) {
                logLock ();
                log_locked__ (LogLevel::Debug, _this_func, "bad request #1: \"", mem, "\"");
                logHex_locked__ (LogLevel::Debug, mem, _this_func);
                logUnlock ();
            }
            return Result::Failure;
        }
        method_end = mem.buf() + offs;

        for (; offs < mem.len(); ++offs) {
            if (   mem.buf() [offs] != 32 /* SP */
                && mem.buf() [offs] !=  9 /* HT */)
            {
                break;
            }
        }
        if (offs == mem.len()) {
            if (logLevelOn (http, LogLevel::Debug)) {
                logLock ();
                log_locked__ (LogLevel::Debug, _this_func, "bad request #2: \"", mem, "\"");
                logHex_locked__ (LogLevel::Debug, mem, _this_func);
                logUnlock ();
            }
            return Result::Failure;
        }
        path_beg = mem.buf() + offs;

        for (; offs < mem.len(); ++offs) {
            if (   mem.buf() [offs] == 32 /* SP */
                || mem.buf() [offs] ==  9 /* HT */)
            {
                break;
            }
        }
        if (offs == mem.len()) {
            if (logLevelOn (http, LogLevel::Debug)) {
                logLock ();
                log_locked__ (LogLevel::Debug, _this_func, "bad request #3: \"", mem, "\"");
                logHex_locked__ (LogLevel::Debug, mem, _this_func);
                logUnlock ();
            }
            return Result::Failure;
        }
        uri_end = mem.buf() + offs;

        for (; offs < mem.len(); ++offs) {
            if (   mem.buf() [offs] != 32 /* SP */
                && mem.buf() [offs] !=  9 /* HT */)
            {
                break;
            }
        }
        if (offs == mem.len()) {
            if (logLevelOn (http, LogLevel::Debug)) {
                logLock ();
                log_locked__ (LogLevel::Debug, _this_func, "bad request #4: \"", mem, "\"");
                logHex_locked__ (LogLevel::Debug, mem, _this_func);
                logUnlock ();
            }
            return Result::Failure;
        }
        proto_beg = mem.buf() + offs;

        proto_end = mem.buf() + mem.len();
    }

    if (client_mode) {
        ConstMemory const response_code_mem (path_beg, uri_end - path_beg);
        if (!strToUint32 (response_code_mem, &cur_req->response_code, /*ret_endptr=*/ NULL, /*base=*/ 10))
            logD_ (_this_func, "bad response code \"", response_code_mem, "\"");

        logD_ (_this_func, "response code ", cur_req->response_code, " (\"", response_code_mem, "\")");
        logD_ (_this_func, "cur_req 0x", fmt_hex, (UintPtr) cur_req.ptr());
    }

    cur_req->method = ConstMemory (mem.buf(), method_end - mem.buf());

    // Skipping URI start (rtsp://a:b@c:1/), which is equivalent to skipping 3 slashes.
    //
    // TODO Obey the standard/RFC/whatever (HTTP + URI).
    if (   stringHasPrefix (ConstMemory (path_beg, uri_end - path_beg), "http://")
        || stringHasPrefix (ConstMemory (path_beg, uri_end - path_beg), "rtsp://"))
    {
        unsigned num_slashes = 0;
        for (; path_beg < uri_end; ++path_beg) {
            if (*path_beg == '/') {
                ++num_slashes;
                if (num_slashes >= 3) {
                    ++path_beg;
                    break;
                }
            }
        }
    }

    Size path_offs = path_beg - mem.buf();
    if (mem.len() - path_offs >= 1 &&
        mem.buf() [path_offs] == '/')
    {
        ++path_offs;
        ++path_beg;
    }

    {
        Memory const proto_mem (proto_beg, proto_end - proto_beg);
        toUppercase (proto_mem);
        if (equal (proto_mem, "HTTP/1.0")) {
            cur_req->http_version = HttpRequest::HttpVersion_1_0;
            cur_req->keepalive = false;
        } else {
            cur_req->http_version = HttpRequest::HttpVersion_1_1;
            cur_req->keepalive = true;
        }
    }

    Byte * const params_start = (Byte*) memchr (path_beg, '?', uri_end - path_beg);
    Byte const *path_end;
    cur_req->params = ConstMemory ();
    if (params_start) {
        if (uri_end - params_start > 1)
            cur_req->params = ConstMemory (params_start + 1, uri_end - params_start - 1);

        path_end = params_start;
    } else {
        path_end = uri_end;
    }

    cur_req->full_path = ConstMemory (path_beg, path_end - path_beg);

    {
      // Counting path elements.

        Size path_pos = path_offs;
        while (path_pos < (Size) (path_end - mem.buf())) {
            Byte const *next_path_elem = (Byte const *) memchr (mem.buf() + path_pos, '/', mem.len() - path_pos);
            if (next_path_elem > path_end)
                next_path_elem = NULL;

            Size path_elem_end;
            if (next_path_elem)
                path_elem_end = next_path_elem - mem.buf();
            else
                path_elem_end = path_end - mem.buf();

            ++cur_req->num_path_elems;

            path_pos = path_elem_end + 1;
        }

        // Trailing '/' adds an extra empty path element.
        if (path_pos > 0 && path_pos - 1 < mem.len() && mem.buf() [path_pos - 1] == '/')
            ++cur_req->num_path_elems;
    }

    if (cur_req->num_path_elems > 0) {
      // Filling path elements.

        cur_req->path = new (std::nothrow) ConstMemory [cur_req->num_path_elems];
        assert (cur_req->path);

        Size path_pos = path_offs;
        Count index = 0;
        while (path_pos < (Size) (path_end - mem.buf())) {
            Byte const *next_path_elem = (Byte const *) memchr (mem.buf() + path_pos, '/', mem.len() - path_pos);
            if (next_path_elem > path_end)
                next_path_elem = NULL;

            Size path_elem_end;
            if (next_path_elem)
                path_elem_end = next_path_elem - mem.buf();
            else
                path_elem_end = path_end - mem.buf();

            cur_req->path [index] = ConstMemory (mem.buf() + path_pos, path_elem_end - path_pos);
            ++index;

            path_pos = path_elem_end + 1;
        }
    }

    if (params_start) {
      // Parsing request parameters.
        Byte * const param_pos = params_start + 1; // Skipping '?'
        if (param_pos < uri_end)
            cur_req->parseParameters (Memory (param_pos, uri_end - param_pos));
    }

    if (logLevelOn (http, LogLevel::Debug)) {
        logD (http, _this_func, "request line: ", cur_req->getRequestLine ());
        logD (http, _this_func, "method: ", cur_req->getMethod ());
        logD (http, _this_func, "full path: ", cur_req->getFullPath ());
        logD (http, _this_func, "path elements (", cur_req->getNumPathElems(), "):");
        for (Count i = 0, i_end = cur_req->getNumPathElems(); i < i_end; ++i)
            logD (http, _this_func, cur_req->getPath (i));
    }

    return Result::Success;
}

typedef Result (*ProcessTokenList_Callback) (Memory  token,
                                             void   *cb_data);

static Result processTokenList (Memory                      const token_list_mem,
                                ProcessTokenList_Callback   const mt_nonnull cb,
                                void                      * const cb_data)
{
    Size pos = 0;
    for (;;) {
        HttpParser::skipLWS (token_list_mem.region (pos), &pos);

        {
            Size const token_start = pos;
            logD (http, _func, "token region: ", token_list_mem.region (pos));
            skipToken (token_list_mem.region (pos), &pos);
            logD (http, _func, "token_start ", token_start, " pos ", pos);
            if (token_start != pos) {
                if (cb) {
                    if (!cb (token_list_mem.region (token_start, pos - token_start), cb_data))
                        return Result::Failure;
                }
            }
        }

        HttpParser::skipLWS (token_list_mem.region (pos), &pos);

      // Stop if there's no comma

        if (pos >= token_list_mem.len())
            break;

        if (token_list_mem.buf() [pos] != ',')
            break;

        ++pos;
    }

    return Result::Success;
}

mt_sync_domain (receiver_frontend) Result
HttpServer::processHeaderField_TransferEncoding (Memory const transfer_encoding_mem)
{
    Size pos = 0;
    for (;;) {
        HttpParser::skipLWS (transfer_encoding_mem.region (pos), &pos);

        {
            Size const token_start = pos;
            skipToken (transfer_encoding_mem.region (pos), &pos);
            if (token_start != pos) {
                Memory const token_mem (transfer_encoding_mem.region (token_start, pos - token_start));
                toLowercase (token_mem);

                if (equal (token_mem, "chunked")) {
                    logD (http, _func, "chunked");
                    cur_req->chunked_transfer_coding = true;
                }

                HttpParser::skipLWS (transfer_encoding_mem.region (pos), &pos);

                if (pos >= transfer_encoding_mem.len())
                    break;

                {
                    Byte * const comma_ptr = (Byte*) memchr (transfer_encoding_mem.buf() + pos,
                                                             ',',
                                                             transfer_encoding_mem.len() - pos);
                    if (!comma_ptr)
                        break;

                    pos = comma_ptr - transfer_encoding_mem.buf();
                }
            }
        }

        HttpParser::skipLWS (transfer_encoding_mem.region (pos), &pos);

        if (pos >= transfer_encoding_mem.len())
            break;

        if (transfer_encoding_mem.buf() [pos] != ',')
            break;

        ++pos;
    }

    return Result::Success;
}

mt_sync_domain (receiver_frontend) Result
HttpServer::processHeaderField_Connection (Memory   const connection_token,
                                           void   * const mt_nonnull _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    logD (http, _self_func, "connection_token: ", connection_token);
    toLowercase (connection_token);

    if (equal (connection_token, "close")) {
        logD (http, _self_func, "close");
        self->cur_req->keepalive = false;
    } else
    if (equal (connection_token, "keep-alive")) {
        logD (http, _self_func, "keep-alive");
        self->cur_req->keepalive = true;
    }

    return Result::Success;
}

mt_sync_domain (receiver_frontend) Result
HttpServer::processHeaderField (Memory mem)
{
    logD (http, _this_func, "mem: ", mem);

    mem = stripLeadTrailWhsp (mem);

    Byte * const header_name_end = (Byte*) memchr (mem.buf(), ':', mem.len());
    if (!header_name_end) {
        if (logLevelOn (http, LogLevel::Error)) {
            logLock ();
            log_locked__ (LogLevel::Error, _this_func, "bad header line: ", mem);
            logHex_locked__ (LogLevel::Error, mem, _this_func);
            logUnlock ();
        }
        return Result::Failure;
    }

    Size header_name_len = header_name_end - mem.buf();

    Byte *header_value_buf = header_name_end + 1;
    Size  header_value_len = mem.len() - header_name_len - 1;
    while (header_value_len > 0
           && (   header_value_buf [0] == 32 /* SP */
               || header_value_buf [0] ==  9 /* HT */
               || header_value_buf [0] == 13 /* CR */
               || header_value_buf [0] == 10 /* LF */))
    {
        ++header_value_buf;
        --header_value_len;
    }
    while (header_value_len > 0
           && (   header_value_buf [header_value_len - 1] == 32 /* SP */
               || header_value_buf [header_value_len - 1] ==  9 /* HT */
               || header_value_buf [header_value_len - 1] == 13 /* CR */
               || header_value_buf [header_value_len - 1] == 10 /* LF */))
    {
        --header_value_len;
    }
    Memory const header_value (header_value_buf, header_value_len);

    while (header_name_len > 0
           && (   mem.buf() [header_name_len - 1] == 32 /* SP */
               || mem.buf() [header_name_len - 1] ==  9 /* HT */
               || mem.buf() [header_name_len - 1] == 13 /* CR */
               || mem.buf() [header_name_len - 1] == 10 /* LF */))
    {
        --header_name_len;
    }
    Memory const header_name (mem.buf(), header_name_len);
    toLowercase (header_name);

    if (put_headers_to_hash) {
        HttpRequest::Header * const header = new (std::nothrow) HttpRequest::Header;
        assert (header);
        header->name  = st_grab (new (std::nothrow) String (header_name));
        header->value = st_grab (new (std::nothrow) String (header_value));

        ++recv_num_headers;
        if (recv_num_headers > max_num_headers) {
            logE (http, _this_func,
                  "recv_num_headers ", recv_num_headers, " > "
                  "max_num_headers ", max_num_headers);
            return Result::Failure;
        }

        cur_req->header_hash.add (header);
    }

    if (equal (header_name, "transfer-encoding")) {
        processHeaderField_TransferEncoding (header_value);
    } else
    if (equal (header_name, "connection")) {
        logD (http, _this_func, "Connection header: ", header_value);
        processTokenList (header_value, processHeaderField_Connection, this);
    } else
    if (equal (header_name, "host")) {
        cur_req->host = st_grab (new (std::nothrow) String (header_value));
    } else
    if (equal (header_name, "content-length")) {
        if (!cur_req->chunked_transfer_coding) {
            recv_content_length = strToUlong (header_value);
            recv_content_length_specified = true;

            cur_req->content_length = recv_content_length;
            cur_req->content_length_specified = true;

            logD (http, _this_func, "recv_content_length: ", recv_content_length);
        } else {
            logD (http, _this_func, "ignoring content-length: chunked coding");
        }
    } else
    if (equal (header_name, "expect")) {
        if (equal (header_value, "100-continue")) {
            if (sender) {
                logD (http, _this_func, "responding to 100-continue");
                sender->send (page_pool,
                              true /* do_flush */,
                              "HTTP/1.1 100 Continue\r\n"
                              "\r\n");
              #if 0
              // Deprecated, maybe wrong
                sender->send (
                        page_pool,
                        true /* do_flush */,
                        "HTTP/1.1 100 Continue\r\n"
                        "Cache-Control: no-cache\r\n"
                        "Content-Type: application/x-fcs\r\n"
                        "Content-Length: 0\r\n"
                        "Connection: Keep-Alive\r\n"
                        "\r\n");
              #endif
            } else {
                logW_ (_this_func, "sender is not set");
            }
        }
    } else
    if (equal (header_name, "accept-language")) {
        cur_req->accept_language = st_grab (new (std::nothrow) String (header_value));
    } else
    if (equal (header_name, "if-modified-since")) {
        cur_req->if_modified_since = st_grab (new (std::nothrow) String (header_value));
    } else
    if (equal (header_name, "if-none-match")) {
        cur_req->if_none_match = st_grab (new (std::nothrow) String (header_value));
    }

    return Result::Success;
}

mt_sync_domain (receiver_frontend) Receiver::ProcessInputResult
HttpServer::receiveRequestLine (Memory const _mem,
                                Size * const mt_nonnull ret_accepted,
                                bool * const mt_nonnull ret_header_parsed)
{
    if (logLevelOn (http, LogLevel::Debug)) {
        logLock ();
        log_locked__ (LogLevel::Debug, _this_func, _mem.len(), " bytes");
        logHex_locked__ (LogLevel::Debug, _mem, _this_func);
        logUnlock ();
    }

    *ret_accepted = 0;
    *ret_header_parsed = false;

    Memory mem = _mem;

    Size field_offs = 0;
    for (;;) {
        logD (http, _this_func, "iteration, mem.len(): ", mem.len(), ", recv_pos: ", recv_pos);

        assert (mem.len() >= recv_pos);
        if (mem.len() == recv_pos) {
          // No new data since the last input event => nothing changed.
            return Receiver::ProcessInputResult::Again;
        }

        Size cr_pos;
        for (;;) {
            Byte const * const cr_ptr = (Byte const *) memchr (mem.buf() + recv_pos, 13 /* CR */, mem.len() - recv_pos);
            if (!cr_ptr) {
                recv_pos = mem.len();
                logD (http, _this_func, "returning Again #1, recv_pos: ", recv_pos);
                return Receiver::ProcessInputResult::Again;
            }

            cr_pos = cr_ptr - mem.buf();
            // We need LF and one non-SP symbol to determine header field end.
            // Also we want to look 2 symbols ahead to detect end of message headers.
            // This means that we need 3 symbols of lookahead.
            if (mem.len() - (cr_pos + 1) < 3) {
                // Leaving CR unaccepted for the next input event.
                recv_pos = cr_pos;
                logD (http, _this_func, "returning Again #2, recv_pos: ", recv_pos);
                return Receiver::ProcessInputResult::Again;
            }

            if (mem.buf() [cr_pos + 1] == 10 /* LF */ &&
                    // Request line cannot be split into multiple lines.
                    (req_state == RequestState::RequestLine ||
                            (mem.buf() [cr_pos + 2] != 32 /* SP */ &&
                             mem.buf() [cr_pos + 2] !=  9 /* HT */)))
            {
              // Got a complete header field.
                break;
            }

          // CR at cr_pos does not end header field.
          // Searching for another one.
            recv_pos = cr_pos + 1;
            logD (http, _this_func, "new recv_pos: ", recv_pos);
        }

        {
            Uint64 const new_headers_len = recv_headers_len + cr_pos + 2;
            if (   new_headers_len < recv_headers_len
                || new_headers_len > max_headers_len)
            {
                logE (http, _this_func,
                      "recv_headers_len ", new_headers_len, " > "
                      "max_headers_len ", max_headers_len);
                // TODO Send 'bad request' reply here and in similar bad-input places?
                return Receiver::ProcessInputResult::Error;
            }
            recv_headers_len = new_headers_len;
        }

        bool empty_request_line = false;
        if (req_state == RequestState::RequestLine) {
            if (!processRequestLine (mem.region (0, cr_pos), &empty_request_line)) {
                logD (http, _this_func, "processRequestLine() failed");
                return Receiver::ProcessInputResult::Error;
            }

            if (!empty_request_line)
                req_state = RequestState::HeaderField;
        } else {
            if (!processHeaderField (mem.region (0, cr_pos))) {
                logD (http, _this_func, "processHeaderField() failed");
                return Receiver::ProcessInputResult::Error;
            }
        }

        Size const next_pos = cr_pos + 2;
        *ret_accepted = field_offs + next_pos;

        recv_pos = 0;

        if (!empty_request_line) {
            if (mem.buf() [cr_pos + 2] == 13 /* CR */ &&
                mem.buf() [cr_pos + 3] == 10 /* LF */)
            {
                bool block_input = false;

                if (frontend && frontend->request)
                    frontend.call (frontend->request, /*(*/ cur_req, &block_input /*)*/);

                if (block_input)
                    input_blocked_by_user = true;

                *ret_accepted += 2;
                *ret_header_parsed = true;
//#error Returning Again here seems wrong. The buffer may contain another complete request,
//#error and we act like we need more data, thus skipping to another input event, which might not happen.
//#error Check other ProcessInputResult::Again returns as well.
                return Receiver::ProcessInputResult::Again;
            }
        }

        field_offs += next_pos;
        mem = mem.region (next_pos);
    }

    unreachable ();
}

mt_sync_domain (receiver_frontend) void
HttpServer::resetRequestState ()
{
    recv_pos = 0;
    recv_content_length = 0;
    recv_content_length_specified = false;
    recv_headers_len = 0;
    recv_num_headers = 0;
    recv_chunk_state = ChunkState_ChunkHeader;
    recv_chunk_size = 0;
    cur_req = NULL;
    req_state = RequestState::RequestStart;
}

mt_sync_domain (receiver_frontend) Receiver::ProcessInputResult
HttpServer::processInput (Memory   const _mem,
                          Size   * const mt_nonnull ret_accepted,
                          void   * const _self)
{
    logD (http, _func, _mem.len(), " bytes");
//    hexdump (logs, _mem);

    HttpServer * const self = static_cast <HttpServer*> (_self);

    *ret_accepted = 0;

    Memory mem = _mem;
    for (;;) {
        if (self->input_blocked_by_user) {
            logD (http, _self_func, "input_blocked_by_user");
            return Receiver::ProcessInputResult::InputBlocked;
        }

        if (self->input_blocked_by_sender.get() == 1) {
            logD (http, _self_func, "input_blocked_by_sender");
            return Receiver::ProcessInputResult::InputBlocked;
        }

        switch (self->req_state.val()) {
            case RequestState::RequestStart:
                if (self->frontend && self->frontend->rawData) {
                    Size accepted = mem.len();
                    bool req_next = false;
                    bool block_input = false;

                    self->frontend.call (self->frontend->rawData,
                                         /*(*/ mem, &accepted, &req_next, &block_input /*)*/);
                    *ret_accepted += accepted;

                    if (block_input)
                        self->input_blocked_by_user = true;

                    if (!req_next)
                        return Receiver::ProcessInputResult::Again;

                    mem = mem.region (accepted);
                }

                self->req_state = RequestState::RequestLine;

            case RequestState::RequestLine:
                logD (http, _func, "RequestState::RequestLine");
            case RequestState::HeaderField: {
                if (!self->cur_req) {
                    self->cur_req = st_grab (new (std::nothrow) HttpRequest (self->client_mode));
                    self->cur_req->client_addr = self->client_addr;
                }

                bool header_parsed;
                Size line_accepted;
                Receiver::ProcessInputResult const res = self->receiveRequestLine (mem, &line_accepted, &header_parsed);
                *ret_accepted += line_accepted;
                if (!header_parsed)
                    return res;

                mem = mem.region (line_accepted);

                if (self->cur_req->hasBody()) {
                    self->recv_pos = 0;
                    self->req_state = RequestState::MessageBody;
                } else {
                    self->resetRequestState ();
                }
            } break;
            case RequestState::MessageBody: {
                logD (http, _func, "MessageBody, mem.len(): ", mem.len());

                Size toprocess = mem.len();
                bool must_consume = false;
                bool end_of_request = false;

                if (self->cur_req->chunked_transfer_coding) {
                    if (self->recv_chunk_state == ChunkState_ChunkTrailer) {
                      // discarding trailer

                        Byte const * const buf = mem.buf();
                        Size         const len = mem.len();
                        for (Size pos = 0; ; ++pos) {
                            if (len - pos < 4) {
                                if (len > 3)
                                    *ret_accepted += len - 3;

                                return Receiver::ProcessInputResult::Again;
                            }

                            if (   buf [pos + 0] == 13 /* CR */
                                && buf [pos + 1] == 10 /* LF */
                                && buf [pos + 2] == 13 /* CR */
                                && buf [pos + 3] == 10 /* LF */)
                            {
                                *ret_accepted += pos + 4;
                                mem = mem.region (pos + 4);

                                self->resetRequestState ();
                                break;
                            }
                        }

                        continue;
                    }

                    if (self->recv_chunk_state == ChunkState_ChunkBodyCRLF) {
                        Byte const * const buf = mem.buf();
                        Size         const len = mem.len();
                        // The request is invlaid if chunk body is not immediately followed by CRLF,
                        // but we accept more inputs by discarding all data before the next CRLF.
                        for (Size pos = 0; ; ++pos) {
                            if (len - pos < 2)
                                return Receiver::ProcessInputResult::Again;

                            if (   buf [pos + 0] == 13 /* CR */
                                && buf [pos + 1] == 10 /* LF */)
                            {
                                *ret_accepted += pos + 2;
                                mem = mem.region (pos + 2);

                                self->recv_chunk_state = ChunkState_ChunkHeader;
                                break;
                            }
                        }
                    }

                    if (self->recv_chunk_state == ChunkState_ChunkHeader) {
                        Byte const * const buf = mem.buf();
                        Size         const len = mem.len();
                        for (Size pos = 0; ; ++pos) {
                            if (len - pos < 2)
                                return Receiver::ProcessInputResult::Again;

                            if (   buf [pos + 0] == 13 /* CR */
                                && buf [pos + 1] == 10 /* LF */)
                            {
                                self->recv_chunk_size = strToUlong (ConstMemory (buf, pos), 16);
                                logD (http, _self_func, "recv_chunk_size ", self->recv_chunk_size);

                                *ret_accepted += pos + 2;
                                mem = mem.region (pos + 2);

                                if (self->recv_chunk_size == 0) {
                                    end_of_request = true;
                                    self->recv_chunk_state = ChunkState_ChunkTrailer;
                                } else {
                                    self->recv_chunk_state = ChunkState_ChunkBody;
                                }

                                break;
                            }
                        }
                    }

                    if (toprocess >= self->recv_chunk_size - self->recv_pos) {
                        toprocess  = self->recv_chunk_size - self->recv_pos;
                        must_consume = true;
                    }
                } else {
                    if (self->recv_content_length_specified
                        && toprocess >= self->recv_content_length - self->recv_pos)
                    {
                        toprocess = self->recv_content_length - self->recv_pos;
                        must_consume = true;
                        end_of_request = true;
                    }
                }

                logD (http, _func, "recv_pos: ", self->recv_pos, ", toprocess: ", toprocess);
                Size accepted = toprocess;
                if (self->frontend && self->frontend->messageBody) {
                    bool block_input = false;

                    self->frontend.call (self->frontend->messageBody,
                          /*(*/
                            self->cur_req,
                            Memory (mem.buf(), toprocess),
                            end_of_request,
                            &accepted,
                            &block_input
                          /*)*/);
                    assert (accepted <= toprocess);

                    if (block_input)
                        self->input_blocked_by_user = true;
                }

                if (must_consume && accepted != toprocess) {
                    logW (http, _func, "HTTP data was not processed in full");
                    return Receiver::ProcessInputResult::Error;
                }

                self->recv_pos += accepted;

                *ret_accepted  += accepted;
                mem = mem.region (accepted);

                if (self->cur_req->chunked_transfer_coding) {
                    if (self->recv_chunk_state == ChunkState_ChunkBody) {
                        if (self->recv_pos < self->recv_chunk_size)
                            return Receiver::ProcessInputResult::Again;

                        self->recv_chunk_state = ChunkState_ChunkBodyCRLF;
                    } else {
                        assert (self->recv_chunk_state == ChunkState_ChunkTrailer);
                    }

                    assert (self->recv_pos == self->recv_chunk_size);
                    self->recv_pos = 0;
                    continue;
                } else {
                    if (self->recv_content_length_specified) {
                        if (self->recv_pos < self->recv_content_length)
                            return Receiver::ProcessInputResult::Again;

                        // message body processed in full
                        assert (self->recv_pos == self->recv_content_length);
                    } else {
                        // In "rawData" mode, messages without content-length don't have a body,
                        // and we switch to RequestStart state.
                        // Otherwise, we have a HTTP/1.0-style messages with infinite body.
                        if (! (self->frontend && self->frontend->rawData))
                            return Receiver::ProcessInputResult::Again;
                    }
                }

                logD (http, _func, "message body processed in full");

                self->resetRequestState ();
            } break;
            default:
                unreachable ();
        }
    }
    unreachable ();
}

mt_sync_domain (receiver_frontend) void
HttpServer::processEof (Memory   const /* unprocessed_mem */,
                        void   * const _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    logD (http, _func_);

    if (self->frontend && self->frontend->closed) {
        self->frontend.call (self->frontend->closed,
                             /*(*/ self->cur_req, (Exception*) NULL /* exc_ */ /*)*/);
    }
}

mt_sync_domain (receiver_frontend) void
HttpServer::processError (Exception * const exc_,
                          Memory      const /* unprocessed_mem */,
                          void      * const _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    logD (http, _func_);

    if (self->frontend && self->frontend->closed)
        self->frontend.call (self->frontend->closed, /*(*/ self->cur_req, exc_ /*)*/);
}

void
HttpServer::sendStateChanged (SenderState   const sender_state,
                              void        * const _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    if (sender_state == SenderState::ConnectionReady) {
        logD (http, _self_func, "unblocking input");
        self->input_blocked_by_sender.set (0);
        self->deferred_reg.scheduleTask (&self->sender_ready_task, false /* permanent */);
    } else {
        logD (http, _self_func, "blocking input");
        self->input_blocked_by_sender.set (1);
    }
}

void
HttpServer::senderClosed (Exception * const exc_,
                          void      * const _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    if (exc_)
        logD (http, _self_func, "exception: ", exc_->toString());
    else
        logD (http, _self_func_);

    // TODO Verify that the connection is not closed prematurely when we've' already sent the reply but have not processed input data yet.

    // We want frontend->closed to be called from receiver's synchronization domain
    // (and from the same thread as receiver's callbacks) to avoid races on 'cur_req'.
    if (self->frontend && self->frontend->closed && self->deferred_reg.isValid())
        self->deferred_reg.scheduleTask (&self->closed_task, false /* permanent */);
}

mt_sync_domain (receiver_frontend) bool
HttpServer::closedTask (void * const _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    assert (self->frontend && self->frontend->closed);
    {
      // TODO clone the original exception
        InternalException exc_ (InternalException::UnknownError);
        self->frontend.call (self->frontend->closed, /*(*/ self->cur_req, &exc_ /*)*/);
    }

    self->resetRequestState ();

    return false /* do not reschedule */;
}

mt_sync_domain (receiver_frontend) bool
HttpServer::senderReadyTask (void * const _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    if (   !self->input_blocked_by_sender.get()
        && !self->input_blocked_by_user)
    {
        self->receiver->unblockInput ();
    }

    return false /* do not reschedule */;
}

mt_sync_domain (receiver_frontend) bool
HttpServer::unblockInputTask (void * const _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    self->input_blocked_by_user = false;
    if (!self->input_blocked_by_sender.get())
        self->receiver->unblockInput ();

    return false /* do not reschedule */;
}

void
HttpServer::unblockInput ()
{
    assert (deferred_reg.isValid());
    deferred_reg.scheduleTask (&unblock_input_task, false /* permanent */);
}

mt_const void
HttpServer::init (CbDesc<Frontend> const &frontend,
                  Receiver               * const mt_nonnull receiver,
                  Sender                 * const sender,
                  DeferredProcessor      * const deferred_processor,
                  PagePool               * const page_pool,
                  IpAddress                const client_addr,
                  bool                     const client_mode,
                  bool                     const put_headers_to_hash)
{
    this->frontend = frontend;
    this->receiver = receiver;
    this->sender = sender;
    this->page_pool = page_pool;
    this->client_addr = client_addr;
    this->client_mode = client_mode;
    this->put_headers_to_hash = put_headers_to_hash;

    assert (   (!sender && !deferred_processor && !page_pool)
            || ( sender &&  deferred_processor &&  page_pool));

    deferred_reg.setDeferredProcessor (deferred_processor);

    receiver->setFrontend (CbDesc<Receiver::Frontend> (&receiver_frontend, this, this));

    if (sender)
        sender->setFrontend (CbDesc<Sender::Frontend> (&sender_frontend, this, this));
}

HttpServer::HttpServer (EmbedContainer * const embed_container)
    : Object                (embed_container),
      client_mode           (false),
      input_blocked_by_user (false),
      req_state             (RequestState::RequestStart),
      recv_pos              (0),
      recv_content_length   (0),
      recv_content_length_specified (false),
      max_headers_len       (65536),
      recv_headers_len      (0),
      max_num_headers       (1024),
      recv_num_headers      (0),
      recv_chunk_state      (ChunkState_ChunkHeader),
      recv_chunk_size       (0)
{
    sender_ready_task.cb  = CbDesc<DeferredProcessor::TaskCallback> (senderReadyTask,  this, this);
    unblock_input_task.cb = CbDesc<DeferredProcessor::TaskCallback> (unblockInputTask, this, this);
    closed_task.cb        = CbDesc<DeferredProcessor::TaskCallback> (closedTask,       this, this);
}

HttpServer::~HttpServer ()
{
    if (frontend && frontend->closed) {
        frontend.call (frontend->closed,
                       /*(*/ cur_req, (Exception*) NULL /* exc_ */ /*)*/);
    }
}

}

