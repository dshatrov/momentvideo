#ifndef LIBMOMENT__REALTIME_HLS_CLIENT__H__
#define LIBMOMENT__REALTIME_HLS_CLIENT__H__


#include <moment/vod_source.h>
#include <moment/ts_demuxer.h>


namespace Moment {

class RealtimeHlsClient : public VodSource
{
  private:
    StateMutex mutex;

    mt_const String uri;
    mt_const Ref<ServerThreadContext> thread_ctx;
    mt_const Ref<PagePool> page_pool;

    Embed<HttpClient> http_client;
    TsDemuxer ts_demuxer;

    // 'frontend' is safe to use without synchronization, because the user is
    // allowed to call setFrontend() only when it's safe (no async operations
    // in progress).
    mt_mutex (mutex) Cb<Frontend> frontend;

    mt_const String host;
    mt_const Uint16 port;

    // also means that http_client has been initialized
    mt_mutex (mutex) bool hostname_resolved;

    mt_mutex (mutex) bool start_pending;
    mt_mutex (mutex) bool stopped;

    mt_iface (TsDemuxer::TsHandler)
      static TsDemuxer::TsHandler const ts_handler;

      static Result tsMediaMessage (MediaMessage *msg,
                                    void         *_self);
    mt_iface_end

    mt_iface (HttpClient::HttpResponseHandler)
      static HttpClient::HttpResponseHandler const hls_segment_response_handler;

      static Result hlsSegmentHttpResponse (HttpRequest  *resp,
                                            Memory        msg_body,
                                            void        ** mt_nonnull ret_msg_data,
                                            void         *_self);

      static Result hlsSegmentHttpResponseBody (HttpRequest  *resp,
                                                Memory        mem,
                                                bool          end_of_response,
                                                Size         * mt_nonnull ret_accepted,
                                                void         *msg_data,
                                                void         *_self);
    mt_iface_end

    mt_iface (HttpClient::HttpResponseHandler)
      static HttpClient::HttpResponseHandler const hls_playlist_response_handler;

      static Result hlsPlaylistHttpResponse (HttpRequest  *resp,
                                             Memory        msg_body,
                                             void        ** mt_nonnull ret_msg_data,
                                             void         *_self);
    mt_iface_end

    mt_unlocks (mutex) void doStart ();

    static void hostnameLookupCallback (IpAddress_NoPort *addr,
                                        void             *_self);

    mt_unlocks (mutex) void completeHostnameLookup (IpAddress_NoPort * mt_nonnull addr);

  public:
    mt_iface (VodSource)
      virtual Timers* getTimers () { return thread_ctx->getTimers(); }

      void setFrontend (CbDesc<VodSource::Frontend> const &frontend)
      {
          mutex.lock ();
          this->frontend = frontend;
          mutex.unlock ();
      }

      void start (bool stop_time_enabled,
                  Time stop_time_microsec);
      void stop  ();

      void block  ();
      void resume ();

      void seek (Time seek_pos_microsec);

      bool querySourceInfo (CbDesc<VodSource::QuerySourceInfoCallback> const &cb,
                            VodSourceInfo * mt_nonnull ret_source_info);
    mt_iface_end

    mt_const Result init (ServerThreadContext * mt_nonnull thread_ctx,
                          PagePool            * mt_nonnull page_pool,
                          ConstMemory          uri_mem,
                          CbDesc<VodSource::Frontend> const &frontend);

    RealtimeHlsClient (EmbedContainer *embed_container);
};

}


#endif /* LIBMOMENT__REALTIME_HLS_CLIENT__H__ */

