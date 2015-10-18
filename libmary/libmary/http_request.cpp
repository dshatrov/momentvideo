/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/http_parser.h>

#include <libmary/http_request.h>


namespace M {

static Count countFilePathLen (HttpRequest const * const mt_nonnull req,
                               Count               const first_idx,
                               Count               const num_elems)
{
    assert (num_elems > 0 && req->getNumPathElems() >= first_idx + num_elems);

    Count len = 0;
    for (Count i = first_idx, i_end = first_idx + num_elems; i < i_end; ++i) {
        len += req->getPath (i).len() + 1;
    }
    return len - 1;
}

StRef<String>
HttpRequest::fillFilePath (Count const first_idx,
                           Count const num_elems) const
{
    assert (num_elems > 0 && getNumPathElems() >= first_idx + num_elems);

    Count const len = countFilePathLen (this, first_idx, num_elems);

    StRef<String> const str = st_grab (new (std::nothrow) String (len));
    Byte * const buf = str->buf();
    Size  pos = 0;

    {
        ConstMemory const elem_mem = getPath (first_idx);
        memcpy (buf + pos, elem_mem.buf(), elem_mem.len());
        pos += elem_mem.len();
    }

    for (Count i = first_idx + 1, i_end = first_idx + num_elems; i < i_end; ++i) {
        ConstMemory const elem_mem = getPath (i);

        buf [pos] = '/';
        ++pos;

        memcpy (buf + pos, elem_mem.buf(), elem_mem.len());
        pos += elem_mem.len();
    }

    assert (pos == len);
    return str;
}

static bool isAlpha (unsigned char const c)
{
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z'))
    {
        return true;
    }

    return false;
}

static bool isAlphaOrDash (unsigned char const c)
{
    if (isAlpha (c) || c == '-')
        return true;

    return false;
}

void parseHttpParameters_noDecode (ConstMemory                    const mem,
                                   ParseHttpParameters_Callback   const mt_nonnull param_cb,
                                   void                         * const param_cb_data)
{
    Byte const *uri_end = mem.mem() + mem.len();
    Byte const *param_pos = mem.mem();

    while (param_pos < uri_end) {
        ConstMemory name;
        ConstMemory value;

        Byte const * const name_end = (Byte const *) memchr (param_pos, '&', uri_end - param_pos);
        Byte const *value_start     = (Byte const *) memchr (param_pos, '=', uri_end - param_pos);
        if (value_start && (!name_end || value_start < name_end)) {
            name = ConstMemory (param_pos, value_start - param_pos);
            ++value_start; // Skipping '='

            Byte const *value_end = (Byte const *) memchr (value_start, '&', uri_end - value_start);
            if (value_end) {
                value = ConstMemory (value_start, value_end - value_start);
                param_pos = value_end + 1; // Skipping '&'
            } else {
                value = ConstMemory (value_start, uri_end - value_start);
                param_pos = uri_end;
            }
        } else {
            if (name_end) {
                name = ConstMemory (param_pos, name_end - param_pos);
                param_pos = name_end + 1; // Skipping '&'
            } else {
                name = ConstMemory (param_pos, uri_end - param_pos);
                param_pos = uri_end;
            }
        }

        param_cb (name, value, param_cb_data);
    }
}

namespace {
    struct ParseHttpParameters_DecodePercentEncoding_Data
    {
        ParseHttpParameters_Callback  param_cb;
        void                         *param_cb_data;
    };
}

static void
parseHttpParameters_decodePercentEncoding_paramCallback (ConstMemory   const name,
                                                         ConstMemory   const value,
                                                         void        * const _data)
{
    ParseHttpParameters_DecodePercentEncoding_Data * const data =
            static_cast <ParseHttpParameters_DecodePercentEncoding_Data*> (_data);

    StRef<String> const decoded_name  = decodePercentEncoding (name);
    StRef<String> const decoded_value = decodePercentEncoding (value);

    data->param_cb (decoded_name, decoded_value, data->param_cb_data);
}

void parseHttpParameters_decodePercentEncoding (ConstMemory                    const mem,
                                                ParseHttpParameters_Callback   const param_cb,
                                                void                         * const param_cb_data)
{
    ParseHttpParameters_DecodePercentEncoding_Data data;
    data.param_cb      = param_cb;
    data.param_cb_data = param_cb_data;

    parseHttpParameters_noDecode (mem, parseHttpParameters_decodePercentEncoding_paramCallback, &data);
}

#define LIBMARY__DECODE_HEX_CHAR__ERROR 16

static Byte decodeHexChar (Byte const c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    return LIBMARY__DECODE_HEX_CHAR__ERROR;
}

StRef<String> decodePercentEncoding (ConstMemory const mem)
{
    StRef<String> const str = newString (mem.len());

    Byte * const to_buf = str->mem().buf();
    Size         to_pos = 0;

    Byte const * const from_buf = mem.buf();
    Size         const from_len = mem.len();
    Size               from_pos = 0;

    while (from_pos < mem.len()) {
        if (from_buf [from_pos] == '+') {
            to_buf [to_pos] = ' ';
            ++from_pos;
        } else
        if (from_buf [from_pos] == '%' && from_len - from_pos >= 3) {

            Byte const hi = decodeHexChar (from_buf [from_pos + 1]);
            Byte const lo = decodeHexChar (from_buf [from_pos + 2]);
            if (   hi != LIBMARY__DECODE_HEX_CHAR__ERROR
                && lo != LIBMARY__DECODE_HEX_CHAR__ERROR)
            {
                to_buf [to_pos] = (hi << 4) | lo;
                from_pos += 3;
            } else {
                to_buf [to_pos] = from_buf [from_pos];
                ++from_pos;
            }
        } else {
            to_buf [to_pos] = from_buf [from_pos];
            ++from_pos;
        }

        ++to_pos;
    }

    to_buf [to_pos] = 0;
    str->setLength (to_pos);

    return str;
}

void
HttpRequest::parseParameters_paramCallback (ConstMemory   const name,
                                            ConstMemory   const value,
                                            void        * const _self)
{
    HttpRequest * const self = static_cast <HttpRequest*> (_self);

    HttpRequest::Parameter * const param = new (std::nothrow) HttpRequest::Parameter;
    assert (param);
    param->name  = newString (name);
    param->value = newString (value);

    self->parameter_hash.add (param);
}

void
HttpRequest::parseParameters (Memory const mem)
{
    parseHttpParameters_decodePercentEncoding (mem, parseParameters_paramCallback, this);
}

void
HttpRequest::parseAcceptLanguage (ConstMemory              const mem,
                                  List<AcceptedLanguage> * const mt_nonnull res_list)
{
    Size pos = 0;
    for (;;) {
        HttpParser::skipLWS (mem, &pos);
        if (pos >= mem.len())
            return;

        unsigned long const lang_begin = pos;
        for (; pos < mem.len(); ++pos) {
            if (!isAlphaOrDash (mem.mem() [pos]))
                break;
        }

        ConstMemory const lang = mem.region (lang_begin, pos - lang_begin);
        double weight = 1.0;

        // TODO Doesn't whitespace + ';' turn the rest of the string into a comment?
        HttpParser::skipLWS (mem, &pos);
        do {
            if (pos >= mem.len())
                break;

            if (mem.mem() [pos] == ';') {
                ++pos;
                HttpParser::skipLWS (mem, &pos);
                if (pos >= mem.len())
                    break;

                if (mem.mem() [pos] == 'q') {
                    ++pos;
                    HttpParser::skipLWS (mem, &pos);
                    if (pos >= mem.len())
                        break;

                    if (mem.mem() [pos] == '=') {
                        ++pos;
                        HttpParser::skipLWS (mem, &pos);
                        if (pos >= mem.len())
                            break;

                        if (mem.mem() [pos] == '0') {
                            weight = 0.0;

                            do {
                                ++pos;
                                HttpParser::skipLWS (mem, &pos);
                                if (pos >= mem.len())
                                    break;

                                if (mem.mem() [pos] != '.')
                                    break;

                                ++pos;
                                HttpParser::skipLWS (mem, &pos);
                                if (pos >= mem.len())
                                    break;

                                double const mul_arr [3] = { 0.1, 0.01, 0.001 };
                                unsigned mul_idx = 0;
                                for (; mul_idx < 3; ++mul_idx) {
                                    if (mem.mem() [pos] >= '0' && mem.mem() [pos] <= '9') {
                                        weight += mul_arr [mul_idx] * (mem.mem() [pos] - '0');

                                        ++pos;
                                        HttpParser::skipLWS (mem, &pos);
                                        if (pos >= mem.len())
                                            break;
                                    } else {
                                      // garbage
                                        break;
                                    }
                                }
                            } while (0);
                        } else {
                          // The 'qvalue' is either ("1" ["." 0*3 ("0")])
                          // or some garbage.
                            weight = 1.0;
                        }
                    }
                }
            }

            // Garbage skipping.
            for (; pos < mem.len(); ++pos) {
                if (mem.mem() [pos] == ',')
                    break;
            }
        } while (0);

        if (pos >= mem.len() || mem.mem() [pos] == ',') {
            if (lang.len() > 0) {
                res_list->appendEmpty ();
                AcceptedLanguage * const alang = &res_list->getLast();
                alang->lang = st_grab (new (std::nothrow) String (lang));
                alang->weight = weight;
            }

            if (pos < mem.len())
                ++pos;
        } else {
          // Unreachable because of garbage skipping.
            logW_ (_func, "parse error, Accept-Language: ", mem);
            break;
        }
    }
}

static StRef<String> parseQuotedString (ConstMemory   const mem,
                                        Size        * const mt_nonnull ret_pos)
{
    Size pos = *ret_pos;
    StRef<String> unescaped_str;

    HttpParser::skipLWS (mem, &pos);
    if (pos >= mem.len())
        goto _return;

    if (mem.mem() [pos] != '"')
        goto _return;

    ++pos;
    if (pos >= mem.len())
        goto _return;

    {
        Size const tag_begin = pos;
        for (unsigned i = 0; i < 2; ++i) {
            Size unescaped_len = 0;
            bool escaped = false;
            for (; pos < mem.len(); ++pos) {
                if (!escaped) {
                    if (mem.mem() [pos] == '"')
                        break;

                    if (mem.mem() [pos] == '\\') {
                        escaped = true;
                    } else {
                        if (i == 1)
                            unescaped_str->mem().mem() [unescaped_len] = mem.mem() [pos];

                        ++unescaped_len;
                    }
                } else {
                    escaped = false;
                    ++unescaped_len;
                }
            }

            if (i == 0) {
                unescaped_str = st_grab (new (std::nothrow) String (unescaped_len));
                pos = tag_begin;
            }
        }
    }

_return:
    *ret_pos = pos;
    return unescaped_str;
}

static StRef<String> parseEntityTag (ConstMemory   const mem,
                                     bool        * const ret_weak,
                                     Size        * const mt_nonnull ret_pos)
{
    Size pos = *ret_pos;
    StRef<String> etag_str;

    if (*ret_weak)
        *ret_weak = false;

    HttpParser::skipLWS (mem, &pos);
    if (pos >= mem.len())
        goto _return;

    if (mem.mem() [pos] == 'W') {
        if (*ret_weak)
            *ret_weak = true;

        for (; pos < mem.len(); ++pos) {
            if (mem.mem() [pos] == '"')
                break;
        }

        if (pos >= mem.len())
            goto _return;
    }

    etag_str = parseQuotedString (mem, &pos);

_return:
    *ret_pos = pos;
    return etag_str;
}

void HttpRequest::parseEntityTagList (ConstMemory       const mem,
                                      bool            * const ret_any,
                                      List<EntityTag> * const mt_nonnull ret_etags)
{
    if (ret_any)
        *ret_any = false;

    Size pos = 0;

    HttpParser::skipLWS (mem, &pos);

    if (pos >= mem.len())
        return;

    if (mem.mem() [pos] == '*') {
        if (ret_any)
            *ret_any = true;

        return;
    }

    for (;;) {
        bool weak;
        StRef<String> etag_str = parseEntityTag (mem, &weak, &pos);
        if (!etag_str)
            break;

        ret_etags->appendEmpty ();
        EntityTag * const etag = &ret_etags->getLast();
        etag->etag = etag_str;
        etag->weak = weak;

        HttpParser::skipLWS (mem, &pos);
    }
}

HttpRequest::HttpRequest (bool const client_mode)
    : client_mode    (client_mode),
      response_code  (0),
      http_version   (HttpVersion_1_1),
      chunked_transfer_coding (false),
      path           (NULL),
      num_path_elems (0),
      content_length (0),
      content_length_specified (false),
      keepalive      (true)
{
}

HttpRequest::~HttpRequest ()
{
    delete[] path;

    {
	ParameterHash::iter iter (parameter_hash);
	while (!parameter_hash.iter_done (iter)) {
	    Parameter * const param = parameter_hash.iter_next (iter);
	    delete param;
	}
    }

    {
        HeaderHash::iter iter (header_hash);
        while (!header_hash.iter_done (iter)) {
            Header * const header = header_hash.iter_next (iter);
            delete header;
        }
    }
}

}

