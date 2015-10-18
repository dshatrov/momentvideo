/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__HTTP_SERVER__H__
#define LIBMARY__HTTP_SERVER__H__


#include <libmary/exception.h>
#include <libmary/sender.h>
#include <libmary/receiver.h>
#include <libmary/util_net.h>

#include <libmary/http_request.h>


namespace M {

enum_beg (HttpStatusCode)
    enum Value {
        Ok                  = 200,
        BadRequest          = 400,
        NotFound            = 404,
        InternalServerError = 500
    };
enum_end (HttpStatusCode)

class HttpServer : public Object
{
  public:
    struct Frontend {
        void (*rawData) (Memory   mem,
                         Size   * mt_nonnull ret_accepted,
                         bool   * mt_nonnull ret_req_next,
                         bool   * mt_nonnull ret_block_input,
                         void   * mt_nonnull cb_data);

        void (*request) (HttpRequest * mt_nonnull req,
                         bool        * mt_nonnull ret_block_input,
                         void        *cb_data);

        // Called only when req->hasBody() is true.
        void (*messageBody) (HttpRequest * mt_nonnull req,
                             Memory       mem,
                             bool         end_of_request,
                             Size        * mt_nonnull ret_accepted,
                             bool        * mt_nonnull ret_block_input,
                             void        *cb_data);

        // Note that closed() may be called from any thread.
        // It is not bound to receiver_frontend synchronization domain.
        // TODO ^ make sure that this is taken into account everywhere.
        void (*closed) (HttpRequest *req,
                        Exception   *exc_,
                        void        *cb_data);
    };

  private:
    enum_beg (RequestState)
        enum_values (
            RequestStart,
            RequestLine,
            HeaderField,
            MessageBody
        )
    enum_end (RequestState)

    mt_const Cb<Frontend> frontend;

    mt_const Ref<Receiver>          receiver;
    mt_const Ref<Sender>            sender;
    mt_const Ref<DeferredProcessor> deferred_processor;
    mt_const Ref<PagePool>          page_pool;

    mt_const bool client_mode;
    mt_const bool put_headers_to_hash;

    mt_const IpAddress client_addr;

    DeferredProcessor::Task sender_ready_task;
    DeferredProcessor::Task unblock_input_task;
    DeferredProcessor::Task closed_task;
    DeferredProcessor::Registration deferred_reg;

    AtomicInt input_blocked_by_sender;
    // The user may block receiving of the next request on a keep-alive
    // HTTP connection to be able to send a delayed reply (e.g. for
    // video data transit).
    mt_sync_domain (receiver_frontend) bool input_blocked_by_user;

    mt_sync_domain (receiver_frontend) StRef<HttpRequest> cur_req;

    mt_sync_domain (receiver_frontend) RequestState req_state;
    // Number of bytes received for message body / request line / header field.
    mt_sync_domain (receiver_frontend) Size recv_pos;
    // What the "Content-Length" HTTP header said for the current request.
    mt_sync_domain (receiver_frontend) Size recv_content_length;
    mt_sync_domain (receiver_frontend) bool recv_content_length_specified;

    // total length of all headers
    mt_const Uint64 max_headers_len;
    mt_sync_domain (receiver_frontend) Uint64 recv_headers_len;

    mt_const Count max_num_headers;
    mt_sync_domain (receiver_frontend) Count recv_num_headers;

    enum ChunkState
    {
        ChunkState_ChunkHeader,
        ChunkState_ChunkBody,
        ChunkState_ChunkBodyCRLF,
        ChunkState_ChunkTrailer
    };

    mt_sync_domain (receiver_frontend) ChunkState recv_chunk_state;
    mt_sync_domain (receiver_frontend) Size       recv_chunk_size;

    mt_sync_domain (receiver_frontend)
        Result processRequestLine (Memory  mem,
                                   bool   *ret_empty_request_line);

    mt_sync_domain (receiver_frontend)
        Result processHeaderField_TransferEncoding (Memory transfer_encoding_mem);

    mt_sync_domain (receiver_frontend)
        static Result processHeaderField_Connection (Memory  connection_token,
                                                     void   * mt_nonnull _self);

    mt_sync_domain (receiver_frontend) Result processHeaderField (Memory mem);

    mt_sync_domain (receiver_frontend)
        Receiver::ProcessInputResult receiveRequestLine (Memory _mem,
                                                         Size * mt_nonnull ret_accepted,
                                                         bool * mt_nonnull ret_header_parsed);

    mt_sync_domain (receiver_frontend) void resetRequestState ();

    mt_iface (Receiver::Frontend)
      static Receiver::Frontend const receiver_frontend;

      mt_sync_domain (receiver_frontend)
          static Receiver::ProcessInputResult processInput (Memory  mem,
                                                            Size   * mt_nonnull ret_accepted,
                                                            void   *_self);

      mt_sync_domain (receiver_frontend)
          static void processEof (Memory  unprocessed_mem,
                                  void   *_self);

      mt_sync_domain (receiver_frontend)
          static void processError (Exception *exc_,
                                    Memory     unprocessed_mem,
                                    void      *_self);
    mt_iface_end

    mt_iface (Sender::Frontend)
      static Sender::Frontend const sender_frontend;

      static void sendStateChanged (SenderState  sender_state,
                                    void        *_self);

      static void senderClosed (Exception *exc_,
                                void      *_self);
    mt_iface_end

    mt_sync_domain (receiver_frontend) static bool closedTask (void *_self);

    mt_sync_domain (receiver_frontend) static bool senderReadyTask (void *_self);

    mt_sync_domain (receiver_frontend) static bool unblockInputTask (void *_self);

  public:
    void unblockInput ();

    // @deferred_processor and @receiver should belong to the same thread.
    mt_const void init (CbDesc<Frontend> const &frontend,
                        Receiver               * mt_nonnull receiver,
                        Sender                 *sender             /* may be NULL for client mode */,
                        DeferredProcessor      *deferred_processor /* may be NULL for client mode */,
                        PagePool               *page_pool          /* may be NULL for client mode */,
                        IpAddress               client_addr,
                        bool                    client_mode = false,
                        bool                    put_headers_to_hash = false);

     HttpServer (EmbedContainer *embed_container);
    ~HttpServer ();
};

}


#endif /* LIBMARY__HTTP_SERVER__H__ */

