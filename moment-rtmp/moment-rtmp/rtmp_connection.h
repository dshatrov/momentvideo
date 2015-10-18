/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMOMENT__RTMP_CONNECTION__H__
#define LIBMOMENT__RTMP_CONNECTION__H__


#include <libmary/libmary.h>

#include <moment/inc.h>
#include <moment/libmoment.h>


namespace Moment {

class RtmpConnection : public Object
{
  private:
    // Protects part of receiving state which may be accessed from
    // ~RtmpConnection() destructor.
    // A simple release fence would be enough to synchronize with the destructor.
    // Using a mutex is an overkill. We can switch to using a fence after
    // an implementation of C++0x atomics comes out.
    //
    // It'd be optimal to use a fence just once for each invocation of doProcessInput()
    // and only if there was a change to the part of the state which the destructor
    // needs.
//#warning TODO get rid of in_destr_mutex by malloc'ing' part of RtmpConnection which is accessed
//#warning      from receiver's' sync domain and using DeferredProcessor to schedule its deletion.
    Mutex in_destr_mutex;

    // Protects sending state.
    Mutex send_mutex;

  public:
    struct ConnectionInfo
    {
        bool momentrtmp_proto;
    };

    struct Frontend
    {
	Result (*handshakeComplete) (void *cb_data);

	Result (*commandMessage) (bool            is_data,
                                  MediaMessage   * mt_nonnull msg,
				  Uint32          msg_stream_id,
                                  AmfEncoding     amf_encoding,
                                  ConnectionInfo * mt_nonnull conn_info,
				  void           *cb_data);

	Result (*audioMessage) (AudioMessage * mt_nonnull msg,
				void         *cb_data);

	Result (*videoMessage) (VideoMessage * mt_nonnull msg,
				void         *cb_data);

	void (*senderStateChanged) (SenderState  sender_state,
				    void        *cb_data);

	void (*closed) (Exception *exc_,
			void *cb_data);
    };

    struct Backend
    {
	void (*close) (DisconnectReason  disconnect_reason,
                       void             *cb_data);
    };

  public: // Temporally public
    enum {
	//  3 bytes - chunk basic header;
	// 11 bytes - chunk message header (type 0);
	//  4 bytes - extended timestamp;
	//  3 bytes - fix chunk basic header;
	//  7 bytes - fix chunk message header (type 1).
        //  5 bytes - FLV VIDEODATA packet header
        MaxHeaderLen = 33
    };
  private:

    class ReceiveState
    {
      public:
	enum Value {
	    Invalid,

	    ClientWaitS0,
	    ClientWaitS1,
	    ClientWaitS2,

	    ServerWaitC0,
	    ServerWaitC1,
	    ServerWaitC2,

	    BasicHeader,

	    ChunkHeader_Type0,
	    ChunkHeader_Type1,
	    ChunkHeader_Type2,
	    ChunkHeader_Type3,

	    ExtendedTimestamp,
	    ChunkData
	};
        Value val () const { return value; }
	operator Value () const { return value; }
	ReceiveState (Value const value) : value (value) {}
	ReceiveState () {}
      private:
        operator bool () const; // forbidden
	Value value;
    };

    class CsIdFormat
    {
      public:
	enum Value {
	    Unknown,
	    OneByte,
	    TwoBytes_First,
	    TwoBytes_Second
	};
        Value val () const { return value; }
	operator Value () const { return value; }
	CsIdFormat (Value const value) : value (value) {}
	CsIdFormat () {}
      private:
        operator bool () const; // forbidden
	Value value;
    };

  public:
    class RtmpMessageType
    {
      public:
	enum Value {
	    SetChunkSize      =  1,
	    Abort             =  2,
	    Ack               =  3,
	    UserControl       =  4,
	    WindowAckSize     =  5,
	    SetPeerBandwidth  =  6,
	    AudioMessage      =  8,
	    VideoMessage      =  9,
	    Data_AMF3         = 15,
	    Data_AMF0         = 18,
	    SharedObject_AMF3 = 16,
	    SharedObject_AMF0 = 19,
	    Command_AMF3      = 17,
	    Command_AMF0      = 20,
	    Aggregate         = 22
	};
        Uint32 val () const { return value; }
	operator Uint32 () const { return value; }
	RtmpMessageType (Uint32 const value) : value (value) {}
	RtmpMessageType () {}
	Size toString_ (Memory const &mem, Format const &fmt) const;
      private:
        operator bool () const; // forbidden
	Uint32 value;
    };

    enum {
	CommandMessageStreamId = 0
    };

    enum {
	DefaultMessageStreamId = 1
    };

  private:
    class UserControlMessageType
    {
      public:
	enum Value {
	    StreamBegin      = 0,
	    StreamEof        = 1,
	    StreamDry        = 2,
	    SetBufferLength  = 3,
	    StreamIsRecorded = 4,
	    PingRequest      = 6,
	    PingResponse     = 7,
            BufferEmpty      = 31,
            BufferReady      = 32
	};
        Value val () const { return value; }
	operator Value () const { return value; }
	UserControlMessageType (Value const value) : value (value) {}
	UserControlMessageType () {}
      private:
        operator bool () const; // forbidden
	Value value;
    };

    enum {
	Type0_HeaderLen = 11,
	Type1_HeaderLen =  7,
	Type2_HeaderLen =  3,
	Type3_HeaderLen =  0
    };

  public:
    class ChunkStream : public StReferenced
    {
	friend class RtmpConnection;

      private:
	Uint32 chunk_stream_id;

	// Incoming message accumulator.
	mt_mutex (RtmpConnection::in_destr_mutex) PagePool::PageListHead page_list;

	Size in_msg_offset;

        Uint64 in_msg_timestamp_low;
	Uint64 in_msg_timestamp;
	Uint64 in_msg_timestamp_delta;
	Uint32 in_msg_len;
	Uint32 in_msg_type_id;
	Uint32 in_msg_stream_id;
	bool   in_header_valid;

	mt_mutex (RtmpConnection::send_mutex)
	mt_begin
	  Uint64 out_msg_timestamp;
	  Uint64 out_msg_timestamp_delta;
	  Uint32 out_msg_len;
	  Uint32 out_msg_type_id;
	  Uint32 out_msg_stream_id;
	  bool   out_header_valid;
	mt_end

      public:
	Uint32 getChunkStreamId () const { return chunk_stream_id; }
    };

  private:
    mt_const OutputStream *dump_stream;

    mt_const Ref<Timers>   timers;
    mt_const Ref<PagePool> page_pool;
    mt_const Ref<Sender>   sender;

    mt_const Time send_delay_millisec;
    mt_const Time ping_timeout_millisec;

    mt_const Cb<Frontend> frontend;
    mt_const Cb<Backend> backend;

    mt_mutex (in_destr_mutex) Timers::TimerKey ping_send_timer;
    AtomicInt ping_reply_received;
    AtomicInt ping_timeout_expired_once;

    mt_sync_domain (receiver) Size in_chunk_size;
    mt_mutex (send_mutex) Size out_chunk_size;

    mt_mutex (send_mutex) Size out_got_first_timestamp;
    mt_mutex (send_mutex) Uint64 out_first_timestamp;
    mt_mutex (send_mutex) Count out_first_frames_counter;

    mt_mutex (send_mutex) Time out_last_flush_time;

    mt_sync_domain (receiver) bool extended_timestamp_is_delta;
    mt_sync_domain (receiver) bool ignore_extended_timestamp;

    mt_sync_domain (receiver) bool processing_input;

    typedef AvlTree< StRef<ChunkStream>,
		     MemberExtractor< ChunkStream,
				      Uint32 const,
				      &ChunkStream::chunk_stream_id >,
		     DirectComparator<Uint32> >
	    ChunkStreamTree;

    mt_mutex (in_destr_mutex) ChunkStreamTree chunk_stream_tree;

  // Receiving state

    mt_sync_domain (receiver)
    mt_begin
      Uint32 remote_wack_size;

      Size total_received;
      Size last_ack;

      Uint16 cs_id;
      CsIdFormat cs_id__fmt;
      Size chunk_offset;

      ChunkStream *recv_chunk_stream;

      Byte fmt;

      ReceiveState conn_state;

      // Can be set from 'receiver' when holding 'send_mutex'.
      // For sending; can be set as mt_const at init.
      mt_mutex (send_mutex) bool momentrtmp_proto;
    mt_end

  // Sending state

    mt_const Uint32 local_wack_size;

    static bool timestampGreater (Uint32 const left,
				  Uint32 const right)
    {
	Uint32 delta;
	if (right >= left)
	    delta = right - left;
	else
	    delta = (0xffffffff - left) + right + 1;

	return delta < 0x80000000 ? 1 : 0;
    }

  public:
    mt_sync_domain (receiver) Receiver::ProcessInputResult doProcessInput (ConstMemory  mem,
									   Size        * mt_nonnull ret_accepted);

    struct MessageDesc;

  private:
    mt_mutex (send_mutex) Uint32 mangleOutTimestamp (MessageDesc const * mt_nonnull mdesc);

    static mt_mutex (send_mutex) Size fillMessageHeader (MessageDesc const * mt_nonnull mdesc,
                                                         Size               msg_len,
                                                         ChunkStream       * mt_nonnull chunk_stream,
                                                         Byte              * mt_nonnull header_buf,
                                                         Uint64             timestamp,
                                                         Uint32             prechunk_size,
                                                         bool               momentrtmp_proto);

    mt_sync_domain (receiver) void resetChunkRecvState ();
    mt_sync_domain (receiver) void resetMessageRecvState (ChunkStream * mt_nonnull chunk_stream);

    mt_mutex (in_destr_mutex) void releaseChunkStream (ChunkStream * mt_nonnull chunk_stream);

    MOMENT__RTMP_INC

  public:
    mt_const ChunkStream *control_chunk_stream;
    mt_const ChunkStream *data_chunk_stream;
    mt_const ChunkStream *audio_chunk_stream;
    mt_const ChunkStream *video_chunk_stream;

    mt_one_of(( mt_const, mt_sync_domain (receiver) ))
	ChunkStream* getChunkStream (Uint32 chunk_stream_id,
				     bool create);

  // Send methods.

    // Message description for sending.
    struct MessageDesc
    {
	Uint64 timestamp;
	Uint32 msg_type_id;
	Uint32 msg_stream_id;
	Size msg_len;
	// Chunk stream header compression.
	bool cs_hdr_comp;

        bool adjustable_timestamp;

        MessageDesc ()
            : adjustable_timestamp (false)
        {}

	// TODO ChunkStream *chunk_stream;
    };

    void sendMessage (MessageDesc  const * mt_nonnull mdesc,
		      ChunkStream        * mt_nonnull chunk_stream,
		      ConstMemory         mem,
		      bool                unlocked = false);

    // TODO 'page_pool' parameter is needed.
    void sendMessagePages (MessageDesc const           * mt_nonnull mdesc,
			   ChunkStream                 * mt_nonnull chunk_stream,
			   PagePool::PageListHead      * mt_nonnull page_list,
			   Size                         msg_offset,
			   bool                         take_ownership,
			   bool                         unlocked,
                           Byte const                  *extra_header_buf,
                           unsigned                     extra_header_len,
                           Sender::SenderStateCallback *sender_state_cb = NULL,
                           void                        *sender_state_cb_data = NULL);

    void sendRawPages (PagePool::Page *first_page,
                       Size            data_len);

  // Send utility methods.

  private:
    mt_mutex (mutex) void sendSetChunkSize_locked (Uint32 chunk_size);

    void sendAck (Uint32 seq);

  public:
    void sendWindowAckSize (Uint32 wack_size);

    void sendSetPeerBandwidth (Uint32 wack_size,
			       Byte   limit_type);

    void sendUserControl_BufferReady (Uint32 msg_stream_id);

    void sendUserControl_StreamBegin (Uint32 msg_stream_id);

    void sendUserControl_SetBufferLength (Uint32 msg_stream_id,
					  Uint32 buffer_len);

    void sendUserControl_StreamIsRecorded (Uint32 msg_stream_id);

    void sendUserControl_PingRequest ();

    void sendUserControl_PingResponse (Uint32 timestamp);

    void sendDataMessage_AMF0 (Uint32 msg_stream_id,
                               ConstMemory mem);

    void sendDataMessage_AMF0_Pages (Uint32                  msg_stream_id,
                                     PagePool::PageListHead * mt_nonnull page_list,
                                     Size                    msg_offset,
                                     Size                    msg_len);

    void sendCommandMessage_AMF0 (Uint32 msg_stream_id,
				  ConstMemory const &mem);

    void sendCommandMessage_AMF0_Pages (Uint32                  msg_stream_id,
					PagePool::PageListHead * mt_nonnull page_list,
					Size                    msg_offset,
					Size                    msg_len);

  // Extra send utility methods.

    void sendVideoMessage (VideoMessage                * mt_nonnull msg,
                           Sender::SenderStateCallback *sender_state_cb = NULL,
                           void                        *sender_state_cb_data = NULL);

    void sendAudioMessage (AudioMessage                * mt_nonnull msg,
                           Sender::SenderStateCallback *sender_state_cb = NULL,
                           void                        *sender_state_cb_data = NULL);

    void sendConnect (ConstMemory app_name,
                      ConstMemory page_url,
                      ConstMemory swf_url,
                      ConstMemory tc_url);

    void sendCreateStream ();

    void sendPlay (ConstMemory stream_name);

    void sendPublish (ConstMemory stream_name,
                      ConstMemory record_mode);

  // ______

    void closeAfterFlush ();

    void close ();

    // Useful for controlling RtmpConnection's state from the backend.
    void close_noBackendCb ();

  private:
  // Ping timer

    mt_const void beginPings ();

    static void pingTimerTick (void *_self);

  // ___

    mt_sync_domain (receiver) Result processMessage (ChunkStream *chunk_stream);

    mt_sync_domain (receiver) Result callCommandMessage (ChunkStream *chunk_stream,
							 AmfEncoding  amf_encoding,
                                                         bool         is_data);

    mt_sync_domain (receiver) Result processUserControlMessage (ChunkStream *chunk_stream);

    mt_iface (Sender::Frontend)
      static Sender::Frontend const sender_frontend;

      static void senderStateChanged (SenderState  sender_state,
				      void        *_self);

      static void senderClosed (Exception *exc_,
				void      *_self);
    mt_iface_end

    mt_iface (Receiver::Frontend)
      static Receiver::Frontend const receiver_frontend;

      mt_sync_domain (receiver)
      mt_begin
	static Receiver::ProcessInputResult processInput (Memory  mem,
							  Size   * mt_nonnull ret_accepted,
							  void   *_self);

	static void processEof (Memory  unprocessed_mem,
                                void   *_self);

	static void processError (Exception *exc_,
                                  Memory     unprocessed_mem,
				  void      *_self);
      mt_end
    mt_iface_end

    void doError (Exception *exc_);

  public:
    // TODO setReceiver()
    CbDesc<Receiver::Frontend> getReceiverFrontend ()
        { return CbDesc<Receiver::Frontend> (&receiver_frontend, this, this); }

    mt_const void setFrontend (CbDesc<Frontend> const &frontend)
        { this->frontend = frontend; }

    mt_const void setBackend (CbDesc<Backend> const &backend)
        { this->backend = backend; }

    mt_const void setSender (Sender * const sender)
    {
	this->sender = sender;
	sender->setFrontend (CbDesc<Sender::Frontend> (&sender_frontend, this, this));
    }

    Sender* getSender () const { return sender; }

    mt_const void startClient ();
    mt_const void startServer ();

    // Should be called from frontend->commandMessage() callback only.
    Uint32 getLocalWackSize () const { return local_wack_size; }

    // Should be called from frontend->commandMessage() callback only.
    Uint32 getRemoteWackSize () const { return remote_wack_size; }

  // TODO doCreateStream(), etc. belong to mod_rtmp

    Result doCreateStream (Uint32      msg_stream_id,
			   AmfDecoder * mt_nonnull amf_decoder);

    Result doReleaseStream (Uint32      msg_stream_id,
			    AmfDecoder * mt_nonnull amf_decoder);

    Result doCloseStream (Uint32      msg_steam_id,
			  AmfDecoder * mt_nonnull amf_decoder);

    Result doDeleteStream (Uint32      msg_stream_id,
			   AmfDecoder * mt_nonnull amf_decoder);

    // Parses transaction_id from amf_decoder and sends a basic reply.
    Result doBasicMessage (Uint32      msg_stream_id,
	    		   AmfDecoder * mt_nonnull amf_decoder);

    Result fireVideoMessage (VideoMessage * mt_nonnull video_msg);

    void reportError ();

    Timers* getTimers () const { return timers; }

    mt_const void init (OutputStream *dump_stream,
                        Timers       * mt_nonnull timers,
			PagePool     * mt_nonnull page_pool,
			Time          send_delay_millisec,
                        Time          ping_timeout_millisec,
                        bool          momentrtmp_proto);

     RtmpConnection (EmbedContainer *embed_container);
    ~RtmpConnection ();
};

}


#endif /* LIBMOMENT__RTMP_CONNECTION__H__ */

