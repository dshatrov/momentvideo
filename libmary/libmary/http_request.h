/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__HTTP_REQUEST__H__
#define LIBMARY__HTTP_REQUEST__H__


#include <libmary/list.h>
#include <libmary/hash.h>
#include <libmary/st_referenced.h>
#include <libmary/util_net.h>


namespace M {

typedef void (*ParseHttpParameters_Callback) (ConstMemory  name,
                                              ConstMemory  value,
                                              void        *cb_data);

void parseHttpParameters_noDecode (ConstMemory                   mem,
                                   ParseHttpParameters_Callback   mt_nonnull param_cb,
                                   void                         *param_cb_data);

void parseHttpParameters_decodePercentEncoding (ConstMemory                   mem,
                                                ParseHttpParameters_Callback   mt_nonnull param_cb,
                                                void                         *param_cb_data);

StRef<String> decodePercentEncoding (ConstMemory mem);

class HttpServer;

class HttpRequest : public StReferenced
{
    friend class HttpServer;

  private:
    enum HttpVersion {
        HttpVersion_1_0 = 0,
        HttpVersion_1_1
    };

    class Parameter : public HashEntry<>
    {
      public:
        StRef<String> name;
        StRef<String> value;
    };

    typedef Hash< Parameter,
                  ConstMemory,
                  MemberExtractor< Parameter,
                                   StRef<String>,
                                   &Parameter::name,
                                   ConstMemory,
                                   AccessorExtractor< String,
                                                      Memory,
                                                      &String::mem > >,
                  MemoryComparator<> >
            ParameterHash;

    class Header : public HashEntry<>
    {
      public:
        StRef<String> name;
        StRef<String> value;
    };

    typedef Hash< Header,
                  ConstMemory,
                  MemberExtractor< Header,
                                   StRef<String>,
                                   &Header::name,
                                   ConstMemory,
                                   AccessorExtractor< String,
                                                      Memory,
                                                      &String::mem > >,
                  MemoryComparator<> >
            HeaderHash;

    bool client_mode;
    Uint32 response_code;

    StRef<String> request_line;
    HttpVersion   http_version;
    bool          chunked_transfer_coding;
    ConstMemory   method;
    ConstMemory   full_path;
    ConstMemory   params;
    ConstMemory  *path;
    Count         num_path_elems;

    StRef<String> host;

    // TODO Support X-Real-IP and alikes.
    IpAddress     client_addr;
    Uint64        content_length;
    bool          content_length_specified;
    StRef<String> accept_language;
    StRef<String> if_modified_since;
    StRef<String> if_none_match;

    ParameterHash parameter_hash;
    HeaderHash    header_hash;

    bool keepalive;

  public:
    Uint32      getResponseCode    () const { return response_code; }
    HttpVersion getHttpVersion     () const { return http_version; }
    ConstMemory getRequestLine     () const { return String::mem (request_line); }
    ConstMemory getMethod          () const { return method; }
    ConstMemory getFullPath        () const { return full_path; }
    ConstMemory getParams          () const { return params; }
    Count       getNumPathElems    () const { return num_path_elems; }
    ConstMemory getHost            () const { return String::mem (host); }
    IpAddress   getClientAddress   () const { return client_addr; }
    Uint64      getContentLength   () const { return content_length; }
    bool        getContentLengthSpecified () const { return content_length_specified; }
    ConstMemory getAcceptLanguage  () const { return String::mem (accept_language); }
    ConstMemory getIfModifiedSince () const { return String::mem (if_modified_since); }
    ConstMemory getIfNoneMatch     () const { return String::mem (if_none_match); }

    void setKeepalive (bool const keepalive) { this->keepalive = keepalive; }
    bool getKeepalive () const { return keepalive; }

    bool hasBody () const
    {
        return    chunked_transfer_coding
               || ((http_version == HttpVersion_1_0 || client_mode)
                       && !getContentLengthSpecified())
               || getContentLength() > 0;
    }

    ConstMemory getPath (Count const index) const
    {
        if (index >= num_path_elems)
            return ConstMemory();

        return path [index];
    }

    StRef<String> fillFilePath (Count first_idx,
                                Count num_elems) const;

    // If ret.mem() == NULL, then the parameter is not set.
    // If ret.len() == 0, then the parameter has empty value.
    ConstMemory getParameter (ConstMemory const name)
    {
        Parameter * const param = parameter_hash.lookup (name);
        if (!param)
            return ConstMemory();

        return param->value;
    }

    ConstMemory getHeader (ConstMemory const name)
    {
        Header * const param = header_hash.lookup (name);
        if (!param)
            return ConstMemory();

        return param->value->mem();
    }

    bool hasParameter (ConstMemory const name) { return parameter_hash.lookup (name); }
    bool hashHeader   (ConstMemory const name) { return header_hash.lookup (name); }

  private:
    static void parseParameters_paramCallback (ConstMemory  name,
                                               ConstMemory  value,
                                               void        *_self);

  public:
    void parseParameters (Memory mem);

    struct AcceptedLanguage
    {
        // Always non-null after parsing.
        StRef<String> lang;
        double weight;
    };

    static void parseAcceptLanguage (ConstMemory             mem,
                                     List<AcceptedLanguage> * mt_nonnull res_list);

    struct EntityTag
    {
        StRef<String> etag;
        bool weak;
    };

    static void parseEntityTagList (ConstMemory      mem,
                                    bool            *ret_any,
                                    List<EntityTag> * mt_nonnull ret_etags);

     HttpRequest (bool client_mode);
    ~HttpRequest ();
};

}


#endif /* LIBMARY__HTTP_REQUEST__H__ */

