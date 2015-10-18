/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include "hmac/hmac_sha2.h"

#include <moment/flv_util.h>

#include <moment-rtmp/rtmp_connection.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_chunk     ("rtmp_chunk",      LogLevel::I);
static LogGroup libMary_logGroup_msg       ("rtmp_msg",        LogLevel::I);
static LogGroup libMary_logGroup_codec     ("rtmp_codec",      LogLevel::I);
static LogGroup libMary_logGroup_send      ("rtmp_send",       LogLevel::I);
static LogGroup libMary_logGroup_time      ("rtmp_time",       LogLevel::I);
static LogGroup libMary_logGroup_close     ("rtmp_conn_close", LogLevel::I);
static LogGroup libMary_logGroup_proto_in  ("rtmp_proto_in",   LogLevel::I);
static LogGroup libMary_logGroup_proto_out ("rtmp_proto_out",  LogLevel::I);

namespace {
    enum {
        // RTMP spec is vague with regard to minimum chunk size.
        // It has SHOULDs all over and then says that chunk size MUST be
        // "at least 1 byte".
        //
        // We require 128 bytes for outgoing chunks, because we don't
        // handle chunking for outgoing RTMP control messages and alikes.
        MinChunkSize = 128,
        // The actual max chunk size is larger.
        MaxChunkSize = 0x7fffffff,
        // Latest version of RTMP spec explicitly allows huge chunks.
        //
        // As of 05.11.13, Flash player appears to misbehave with small chunk sizes:
        // h.264 video playback starts with random noticable delay and may be choppy,
        // either during first seconds of playback, or continuously.
        //
        // Chunking is practically uselsess and thus completely evil.
        //
        // I wasted enough time trying to figure out if there's some mistake
        // in Moment's chunking logics. As far as I can tell, there's nothing
        // wrong with it. Don't waste any more time on chunking!
        PrechunkSize = 0x7fffffff,
        DefaultChunkSize = 128
    };
}


Sender::Frontend const RtmpConnection::sender_frontend = {
    senderStateChanged,
    senderClosed
};

Receiver::Frontend const RtmpConnection::receiver_frontend = {
    processInput,
    processEof,
    processError,
    NULL /* processInputFrom */
};

Size
RtmpConnection::RtmpMessageType::toString_ (Memory const &mem,
					    Format const & /* fmt */) const
{
    switch (value) {
	case SetChunkSize:
	    return toString (mem, "SetChunkSize");
	case Abort:
	    return toString (mem, "Abort");
	case Ack:
	    return toString (mem, "Ack");
	case UserControl:
	    return toString (mem, "UserControl");
	case WindowAckSize:
	    return toString (mem, "WindowAckSize");
	case SetPeerBandwidth:
	    return toString (mem, "SetPeerBandwidth");
	case AudioMessage:
	    return toString (mem, "AudioMessage");
	case VideoMessage:
	    return toString (mem, "VideoMessage");
	case Data_AMF3:
	    return toString (mem, "Data_AMF3");
	case Data_AMF0:
	    return toString (mem, "Data_AMF0");
	case SharedObject_AMF3:
	    return toString (mem, "SharedObject_AMF3");
	case SharedObject_AMF0:
	    return toString (mem, "SharedObject_AMF0");
	case Command_AMF3:
	    return toString (mem, "Command_AMF3");
	case Command_AMF0:
	    return toString (mem, "Command_AMF0");
	case Aggregate:
	    return toString (mem, "Aggregate");
        default:
            return toString (mem, "Unknown");
    }

    unreachable ();
    return 0;
}

mt_one_of(( mt_const, mt_sync_domain (receiver) ))
RtmpConnection::ChunkStream*
RtmpConnection::getChunkStream (Uint32 const chunk_stream_id,
				bool const create)
{
    ChunkStreamTree::Node * const chunk_stream_node = chunk_stream_tree.lookup (chunk_stream_id);
    if (!chunk_stream_node) {
	if (!create)
	    return NULL;

	// TODO Max number of chunk streams.

	StRef<ChunkStream> const chunk_stream = st_grab (new (std::nothrow) ChunkStream);

	chunk_stream->chunk_stream_id = chunk_stream_id;
	chunk_stream->in_msg_offset = 0;
	chunk_stream->in_header_valid = false;
	chunk_stream->out_header_valid = false;

	in_destr_mutex.lock ();
	chunk_stream_tree.add (chunk_stream);
	in_destr_mutex.unlock ();

	return chunk_stream;
    }

    return chunk_stream_node->value;
}

mt_mutex (in_destr_mutex) void
RtmpConnection::releaseChunkStream (ChunkStream * const mt_nonnull chunk_stream)
{
    if (!chunk_stream->page_list.isEmpty ()) {
	page_pool->msgUnref (chunk_stream->page_list.first);
	chunk_stream->page_list.reset ();
    }
}

MOMENT__RTMP_IMPL

mt_mutex (send_mutex) Uint32
RtmpConnection::mangleOutTimestamp (MessageDesc const * const mt_nonnull mdesc)
{
    if (!out_got_first_timestamp) {
        if (mdesc->adjustable_timestamp) {
            out_first_timestamp = mdesc->timestamp;
            out_got_first_timestamp = true;
        }

        return 0;
    }

    Uint64 mangled_timestamp = mdesc->timestamp - out_first_timestamp;

    // TODO What's this?
    //      ^^^ This is likely avoiding sending negative timestamps at the beginning of the stream.
    //          A dirty hack, actually. Timestamps shouldn't wrap this way at the beginning
    //          of a stream. This should be dealt with in VideoStream's bindind logics and alike.
    if (out_first_frames_counter < 1000 /* Artificial limit */) {
        if (mangled_timestamp >
                    (momentrtmp_proto ? (( (Uint64) -60000000000ULL /* 1 minute */) / 1000) :
                                        ((((Uint64) -60000000000ULL /* 1 minute */) / 1000000) & 0xffffffff)))
        {
            mangled_timestamp = 0;
        }

        ++out_first_frames_counter;
    }

    return mangled_timestamp;
}

// @msg_len = mdesc->msg_len + extra header length
mt_mutex (send_mutex) Size
RtmpConnection::fillMessageHeader (MessageDesc const * const mt_nonnull mdesc,
                                   Size                const mt_nonnull msg_len,
				   ChunkStream       * const mt_nonnull chunk_stream,
				   Byte              * const mt_nonnull header_buf,
				   Uint64              const timestamp,
				   Uint32              const prechunk_size,
                                   bool                const momentrtmp_proto)
{
    bool has_extended_timestamp = false;
    Uint64 extended_timestamp = 0;

    Size offs = 0;

    // Basic header (1 byte, filled below).
    offs += 1;

    bool got_header = false;
    Byte header_type = 0;

    logD (time, _func, "timestamp: 0x", fmt_hex, timestamp);

//    logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream,
//	     ", msg_len: ", fmt_def, msg_len);

#if 0
    {
	static int counter = 100;
	if (counter >= 100) {
	    logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream,
		   ", timestamp: 0x", fmt_hex, timestamp);
	    counter = 0;
	} else {
	    ++counter;
	}
    }
#endif

    bool fix_header = false;

    // TODO cs_hdr_comp is probably unnecessary, because all messages
    // for which we explicitly set cs_hdr_comp to 'false' fit into a single 128-byte
    // chunk anyway. Is 128 bytes a minimum size for a chunk?
    if (mdesc->cs_hdr_comp &&
	chunk_stream->out_header_valid)
    {
	bool force_type0 = false;

//#warning TEST Forcing type 0 headers
//	force_type0 = true;

	if (!timestampGreater (chunk_stream->out_msg_timestamp, timestamp)) {
//	    logW_ (_func, "!timestampGreater: ", chunk_stream->out_msg_timestamp, ", ", timestamp);
	    force_type0 = true;
	}

	if (!force_type0 &&
	    chunk_stream->out_msg_stream_id == mdesc->msg_stream_id)
	do {
	    Uint64 const timestamp_delta = timestamp - chunk_stream->out_msg_timestamp;

            // TODO momentrtmp_proto => 7-byte timestamps in microseconds
	    if ((Uint32) timestamp_delta >= 0x00ffffff &&
		prechunk_size &&
		msg_len > prechunk_size)
	    {
	      // Forcing type 0 header.
		fix_header = true;
		break;
	    }

	    if (timestamp < chunk_stream->out_msg_timestamp) {
		// This goes against RTMP rules and should never happen
		// (that's what the timestampGreater() check above is for).
		logW_ (_func, "Backwards timestamp: "
		       "new: ", timestamp, ", "
		       "old: ", chunk_stream->out_msg_timestamp);
	    }

	    if (chunk_stream->out_msg_type_id == mdesc->msg_type_id &&
		chunk_stream->out_msg_len == msg_len)
	    {
		if (chunk_stream->out_msg_timestamp_delta == timestamp_delta &&
		    // We don't want to mix type 3 chunks and extended timestamps.
		    // (There's no well-formulated reason for this.)
		    (Uint32) chunk_stream->out_msg_timestamp < 0x00ffffff)
		{
		  // Type 3 header

		    got_header = true;

//		    logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream, ": "
//			   "type 3 header");

		    chunk_stream->out_msg_timestamp = timestamp;

		    header_type = 3;
		    offs += Type3_HeaderLen;
		} else {
		  // Type 2 header

		    got_header = true;

//		    logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream, ": "
//			   "type 2 header, timestamp_delta: 0x", fmt_hex, timestamp_delta);

		    chunk_stream->out_msg_timestamp = timestamp;
		    chunk_stream->out_msg_timestamp_delta = timestamp_delta;

		    if (!momentrtmp_proto && (Uint32) timestamp_delta >= 0x00ffffff) {
			header_buf [offs + 0] = 0xff;
			header_buf [offs + 1] = 0xff;
			header_buf [offs + 2] = 0xff;

			has_extended_timestamp = true;
			extended_timestamp = timestamp_delta;
		    } else {
			header_buf [offs + 0] = (timestamp_delta >> 16) & 0xff;
			header_buf [offs + 1] = (timestamp_delta >>  8) & 0xff;
			header_buf [offs + 2] = (timestamp_delta >>  0) & 0xff;

                        if (momentrtmp_proto) {
                            has_extended_timestamp = true;
                            extended_timestamp = timestamp_delta;
                        }
		    }

		    header_type = 2;
		    offs += Type2_HeaderLen;
		}
	    }

	    if (!got_header) {
	      // Type 1 header

//		logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream, ": "
//		       "type 1 header");

		got_header = true;

		chunk_stream->out_msg_timestamp = timestamp;
		chunk_stream->out_msg_timestamp_delta = timestamp_delta;
		chunk_stream->out_msg_len = msg_len;
		chunk_stream->out_msg_type_id = mdesc->msg_type_id;

		logD (time, _func, "snd timestamp_delta: 0x", fmt_hex, timestamp_delta);

		if (!momentrtmp_proto && (Uint32) timestamp_delta >= 0x00ffffff) {
		    header_buf [offs + 0] = 0xff;
		    header_buf [offs + 1] = 0xff;
		    header_buf [offs + 2] = 0xff;

		    has_extended_timestamp = true;
		    extended_timestamp = timestamp_delta;
		} else {
		    header_buf [offs + 0] = (timestamp_delta >> 16) & 0xff;
		    header_buf [offs + 1] = (timestamp_delta >>  8) & 0xff;
		    header_buf [offs + 2] = (timestamp_delta >>  0) & 0xff;

                    if (momentrtmp_proto) {
                        has_extended_timestamp = true;
                        extended_timestamp = timestamp_delta;
                    }
		}

		header_buf [offs + 3] = (msg_len >> 16) & 0xff;
		header_buf [offs + 4] = (msg_len >>  8) & 0xff;
		header_buf [offs + 5] = (msg_len >>  0) & 0xff;

		header_buf [offs + 6] = mdesc->msg_type_id;

		header_type = 1;
		offs += Type1_HeaderLen;
	    }
	} while (0);
    }

    if (!got_header) {
      // Type 0 header

//	logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream, ": "
//	       "type 0 header");

	chunk_stream->out_header_valid = true;
	chunk_stream->out_msg_timestamp = timestamp;
	chunk_stream->out_msg_timestamp_delta = timestamp; // Somewhat weird RTMP rule.
	chunk_stream->out_msg_len = msg_len;
	chunk_stream->out_msg_type_id = mdesc->msg_type_id;
	chunk_stream->out_msg_stream_id = mdesc->msg_stream_id;

	logD (time, _func, "snd timestamp: 0x", fmt_hex, timestamp);

	if ((Uint32) timestamp >= 0x00ffffff || momentrtmp_proto) {
	    if (prechunk_size &&
		msg_len > prechunk_size)
	    {
		fix_header = true;
	    }

            if (!momentrtmp_proto) {
                header_buf [offs + 0] = 0xff;
                header_buf [offs + 1] = 0xff;
                header_buf [offs + 2] = 0xff;
            } else {
                header_buf [offs + 0] = (timestamp >> 16) & 0xff;
                header_buf [offs + 1] = (timestamp >>  8) & 0xff;
                header_buf [offs + 2] = (timestamp >>  0) & 0xff;
            }

	    has_extended_timestamp = true;
	    extended_timestamp = timestamp;
	} else {
	    header_buf [offs + 0] = (timestamp >> 16) & 0xff;
	    header_buf [offs + 1] = (timestamp >>  8) & 0xff;
	    header_buf [offs + 2] = (timestamp >>  0) & 0xff;
	}

	if (!fix_header) {
	    header_buf [offs + 3] = (msg_len >> 16) & 0xff;
	    header_buf [offs + 4] = (msg_len >>  8) & 0xff;
	    header_buf [offs + 5] = (msg_len >>  0) & 0xff;

	    header_buf [offs + 6] = mdesc->msg_type_id;
	} else {
	    header_buf [offs + 3] = 0;
	    header_buf [offs + 4] = 0;
	    header_buf [offs + 5] = 0;

	    header_buf [offs + 6] = RtmpMessageType::Data_AMF0;

	    chunk_stream->out_msg_timestamp_delta = 0;
	}

	// Note that msg_stream_id is not in network byte order.
	// This is a deviation from the spec.
	header_buf [offs +  7] = (mdesc->msg_stream_id >>  0) & 0xff;
	header_buf [offs +  8] = (mdesc->msg_stream_id >>  8) & 0xff;
	header_buf [offs +  9] = (mdesc->msg_stream_id >> 16) & 0xff;
	header_buf [offs + 10] = (mdesc->msg_stream_id >> 24) & 0xff;

	header_type = 0;
	offs += Type0_HeaderLen;
    }

    if (has_extended_timestamp) {
//	logD_ (_func, "extended timestamp");

        if (!momentrtmp_proto) {
            header_buf [offs + 0] = (extended_timestamp >> 24) & 0xff;
            header_buf [offs + 1] = (extended_timestamp >> 16) & 0xff;
            header_buf [offs + 2] = (extended_timestamp >>  8) & 0xff;
            header_buf [offs + 3] = (extended_timestamp >>  0) & 0xff;
        } else {
            header_buf [offs + 0] = (extended_timestamp >> 48) & 0xff;
            header_buf [offs + 1] = (extended_timestamp >> 40) & 0xff;
            header_buf [offs + 2] = (extended_timestamp >> 32) & 0xff;
            header_buf [offs + 3] = (extended_timestamp >> 24) & 0xff;
        }

	offs += 4;
    }

    if (fix_header) {
      // This is a workaround for the fact that type 3 chunks have extended
      // timestamp field when current timestamp delta is larger than 0x00ffffff.
      // That breaks prechunking, because different clients require different
      // timestamps for the same message. To workaround this, we use a dummy
      // type 0 chunk whenever we need to send a message which consists of
      // multiple chunks with a large timestamp delta. Then we use a type 1
      // header with timestamp delta 0. As the result, type 3 chunks that follow
      // do not contain extended timestamp field.

	header_buf [offs + 0] = (1 << 6) | (Byte) chunk_stream->chunk_stream_id;
	offs += 1;

	header_buf [offs + 0] = 0;
	header_buf [offs + 1] = 0;
	header_buf [offs + 2] = 0;

	header_buf [offs + 3] = (msg_len >> 16) & 0xff;
	header_buf [offs + 4] = (msg_len >>  8) & 0xff;
	header_buf [offs + 5] = (msg_len >>  0) & 0xff;

	header_buf [offs + 6] = mdesc->msg_type_id;

	offs += 7;
    }

    // FIXME Assuming small chunk stream ids (2-63)
    header_buf [0] = (header_type << 6) | (Byte) chunk_stream->chunk_stream_id;

//    logD_ (_func, "header_type: ", header_type, ", offs: ", offs);
//    logLock ();
//    hexdump (logs, ConstMemory (header_buf, offs));
//    logUnlock ();

    assert (offs <= MaxHeaderLen);
    return offs;
}

void
RtmpConnection::sendMessage (MessageDesc const * const mt_nonnull mdesc,
			     ChunkStream       * const mt_nonnull chunk_stream,
			     ConstMemory         const mem,
			     bool                const unlocked)
{
    PagePool::PageListHead page_list;
    page_pool->getFillPages (&page_list, mem);

    sendMessagePages (mdesc,
                      chunk_stream,
                      &page_list,
                      0    /* msg_offset */,
                      true /* take_ownership */,
                      unlocked, 
                      NULL /* extra_header_buf */,
                      0    /* extra_header_len */);
}

static Size getChunkedMsgLen (Size const msg_len,
                              Size const chunk_size)
{
    if (msg_len > 0)
        return msg_len + (msg_len - 1) / chunk_size;

    return 0;
}

namespace {
class RtmpConnection_ChunkedMessageEntry : public SenderMessageEntry
{
  public:
    Byte header [RtmpConnection::MaxHeaderLen];
    Size base_header_len;
    Size header_len;

    PagePool       *page_pool;
    PagePool::Page *first_page;
    Size            msg_offs;
    Size            msg_len;

    Size chunk_size;

  private:
    PagePool::Page *cur_page;
    Size header_offs;
    Size page_offs;
    Size msg_left;

    Byte chunk_header;
    Size chunk_pos;

  public:
    void init (Uint32 const chunk_stream_id)
    {
        cur_page    = first_page;
        header_offs = 0;
        page_offs   = msg_offs;
        msg_left    = msg_len;

        // TODO Large chunk stream ids.
        assert (chunk_stream_id > 1 && chunk_stream_id < 64);

        chunk_header = (Byte) 0xc0 | (Byte) chunk_stream_id;
        chunk_pos = 0;
    }

    mt_iface (SenderMessageEntry)
      Size getTotalMsgLen () const;

      bool isEmpty ();

      void fillIovs (IovArray *iovs,
                     Size      max_iovs);

      bool react (Size  bytes_sent,
                  Size * mt_nonnull ret_accepted);

      void release ();
    mt_iface_end
};
}

Size
RtmpConnection_ChunkedMessageEntry::getTotalMsgLen () const
{
    return getChunkedMsgLen (msg_len, chunk_size);
}

bool
RtmpConnection_ChunkedMessageEntry::isEmpty ()
{
    return (header_len == 0) && (msg_len == 0);
}

void
RtmpConnection_ChunkedMessageEntry::fillIovs (IovArray * const iovs,
                                              Size       const max_iovs)
{
    Size i = 0;
    Size tmp_chunk_pos = chunk_pos;

    if (header_len > header_offs) {
      // Assuming that header size is less than chunk size.

        iovs->addIov (header + header_offs, header_len - header_offs);
        tmp_chunk_pos = header_len - base_header_len;

        ++i;
        if (i >= max_iovs)
            return;
    }

    PagePool::Page *page = cur_page;
    Size left = msg_left;
    while (left) {
        assert (page);

        Size offs = (page == cur_page ? page_offs : 0);
        Size len = (left <= page->data_len - offs) ? left : page->data_len - offs;

        while (len > 0) {
            if (tmp_chunk_pos >= chunk_size) {
                iovs->addIov (&chunk_header, 1);

                ++i;
                if (i >= max_iovs)
                    return;

                tmp_chunk_pos = 0;
            }

            Size clen = len;
            if (len > chunk_size - tmp_chunk_pos)
                clen = chunk_size - tmp_chunk_pos;

            iovs->addIov (page->getData() + offs, clen);

            ++i;
            if (i >= max_iovs)
                return;

            tmp_chunk_pos += clen;
            left -= clen;

            offs += clen;
            len -= clen;
        }

        page = page->getNextMsgPage();
    }
}

bool
RtmpConnection_ChunkedMessageEntry::react (Size  bytes_sent,
                                           Size * const mt_nonnull ret_accepted)
{
    *ret_accepted = 0;

    if (header_len > header_offs) {
      // Assuming that header size is less than chunk size.

        if (bytes_sent < header_len - header_offs) {
            header_offs   += bytes_sent;
            *ret_accepted += bytes_sent;
            return false;
        }

        chunk_pos = (header_len - base_header_len);

        bytes_sent    -= (header_len - header_offs);
        *ret_accepted += (header_len - header_offs);

        header_offs = header_len;
    }

    PagePool::Page *page = cur_page;
    while (msg_left > 0) {
        assert (page);

        Size len = (msg_left <= page->data_len - page_offs) ? msg_left : page->data_len - page_offs;

        while (len > 0) {
            if (chunk_pos >= chunk_size) {
                if (bytes_sent < 1)
                    return false;

                bytes_sent -= 1;
                chunk_pos = 0;
                *ret_accepted += 1;
            }

            Size clen = len;
            if (len > chunk_size - chunk_pos)
                clen = chunk_size - chunk_pos;

            if (bytes_sent < clen) {
                page_offs     += bytes_sent;
                msg_left      -= bytes_sent;
                chunk_pos     += bytes_sent;
                *ret_accepted += bytes_sent;
                return false;
            }

            bytes_sent    -= clen;
            msg_left      -= clen;
            chunk_pos     += clen;
            *ret_accepted += clen;

            len -= clen;
        }

        PagePool::Page * const next_page = page->getNextMsgPage();

        page_offs = 0;
        cur_page = next_page;
#ifndef LIBMARY_WIN32_IOCP
        page_pool->pageUnref (page);
#endif

        page = next_page;
    }

    return msg_left == 0;
}

void
RtmpConnection_ChunkedMessageEntry::release ()
{
#ifdef LIBMARY_WIN32_IOCP
    PagePool::Page *page = first_page;
    Size left = msg_len;
    Size offs = msg_offs;
#else
    PagePool::Page *page = cur_page;
    Size left = msg_left;
    Size offs = page_offs;
#endif
    if (left > 0) {
        for (;;) {
            assert (page);
            PagePool::Page * const next_page = page->getNextMsgPage();
            Size const len = page->data_len - offs;

            page_pool->pageUnref (page);

            if (left <= len)
                break;

            left -= len;
            page = next_page;
            offs = 0;
        }
    }

    delete this;
}

//#warning page_pool argument
// TODO first_page is enough, page_list not needed
void
RtmpConnection::sendMessagePages (MessageDesc const           * const mt_nonnull mdesc,
				  ChunkStream                 * const mt_nonnull chunk_stream,
				  PagePool::PageListHead      * const mt_nonnull page_list,
				  Size                          const msg_offset,
				  bool                          const take_ownership,
				  bool                          const unlocked,
                                  Byte const                  * const extra_header_buf,
                                  unsigned                      const extra_header_len,
                                  Sender::SenderStateCallback * const sender_state_cb,
                                  void                        * const sender_state_cb_data)
{
    if (!unlocked)
	send_mutex.lock ();

    Uint64 const timestamp = mangleOutTimestamp (mdesc);
    logS_ (_this_func, "adjusted ts ", timestamp, " (was ", mdesc->timestamp, ") ",
           (RtmpMessageType) mdesc->msg_type_id);

    logD (proto_out, _func, "ts 0x", fmt_hex, timestamp, " (orig 0x", mdesc->timestamp, "), "
	  "tid ", fmt_def, mdesc->msg_type_id, ", (", (RtmpMessageType) mdesc->msg_type_id, ")"
	  ", msid ", mdesc->msg_stream_id, ", csid ", chunk_stream->chunk_stream_id,
	  ", mlen ", mdesc->msg_len, ", hdrc ", mdesc->cs_hdr_comp ? "true" : "false");

    RtmpConnection_ChunkedMessageEntry * const msg_entry = new (std::nothrow) RtmpConnection_ChunkedMessageEntry;
    assert (msg_entry);

    msg_entry->header_len = fillMessageHeader (mdesc,
                                               mdesc->msg_len + extra_header_len,
                                               chunk_stream,
                                               msg_entry->header,
                                               timestamp,
                                               PrechunkSize,
                                               momentrtmp_proto);
    msg_entry->base_header_len = msg_entry->header_len;
    if (extra_header_len > 0) {
        assert (msg_entry->header_len + extra_header_len <= MaxHeaderLen);
        memcpy (msg_entry->header + msg_entry->header_len, extra_header_buf, extra_header_len);
        msg_entry->header_len += extra_header_len;
    }

    msg_entry->page_pool  = page_pool;
    msg_entry->first_page = page_list->first;
    msg_entry->msg_offs   = msg_offset;
    msg_entry->msg_len    = mdesc->msg_len;

    if (!take_ownership)
        page_pool->msgRef (page_list->first);

    msg_entry->chunk_size = PrechunkSize;
    if (PrechunkSize != out_chunk_size) {
	out_chunk_size = PrechunkSize;
	sendSetChunkSize_locked (PrechunkSize);
    }

    msg_entry->init (chunk_stream->chunk_stream_id);

    bool do_flush = true;
    {
	Time const cur_time = getTimeMilliseconds();
	if (mdesc->msg_type_id == RtmpMessageType::AudioMessage ||
	    mdesc->msg_type_id == RtmpMessageType::VideoMessage)
	{
	    if (cur_time >= out_last_flush_time
		&& cur_time - out_last_flush_time < send_delay_millisec)
	    {
		do_flush = false;
	    }
	}

	if (do_flush)
	    out_last_flush_time = cur_time;
    }

    logS_ (_this_func, "do_flush: ", do_flush);

    sender->sendMessage (msg_entry, do_flush, sender_state_cb, sender_state_cb_data);

    if (!unlocked)
	send_mutex.unlock ();
}

//#warning page_pool argument
void
RtmpConnection::sendRawPages (PagePool::Page * const first_page,
                              Size             const data_len)
{
    logD (send, _func_);

    SenderMessageEntry_Pages * const msg_pages =
	    SenderMessageEntry_Pages::createNew (0 /* max_header_len */);
    msg_pages->init (first_page, page_pool, /*msg_offs=*/ 0, data_len);

    sender->sendMessage (msg_pages, true /* do_flush */);
}

mt_sync_domain (receiver) void
RtmpConnection::resetChunkRecvState ()
{
    logD (chunk, _func_);

    conn_state = ReceiveState::BasicHeader;
    cs_id = 0;
    cs_id__fmt = CsIdFormat::Unknown;
    chunk_offset = 0;
}

mt_sync_domain (receiver) void
RtmpConnection::resetMessageRecvState (ChunkStream * const mt_nonnull chunk_stream)
{
    logD (send, _func_);

    if (!chunk_stream->page_list.isEmpty ()) {
	page_pool->msgUnref (chunk_stream->page_list.first);

	in_destr_mutex.lock ();
	chunk_stream->page_list.reset ();
	in_destr_mutex.unlock ();
    }

    chunk_stream->in_msg_offset = 0;
}

mt_mutex (mutex) void
RtmpConnection::sendSetChunkSize_locked (Uint32 const chunk_size)
{
    logD (proto_out, _func, chunk_size);

    if (chunk_size < MinChunkSize ||
	chunk_size > MaxChunkSize)
    {
	logW_ (_func, "bad chunk size: ", chunk_size);
    }

    Byte msg [4];

    msg [0] = (chunk_size >> 24) & 0xff;
    msg [1] = (chunk_size >> 16) & 0xff;
    msg [2] = (chunk_size >>  8) & 0xff;
    msg [3] = (chunk_size >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::SetChunkSize;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc,
		 control_chunk_stream,
		 ConstMemory::forObject (msg),
		 true /* unlocked */);
}

void
RtmpConnection::sendAck (Uint32 const seq)
{
    Byte msg [4];

    msg [0] = (seq >> 24) & 0xff;
    msg [1] = (seq >> 16) & 0xff;
    msg [2] = (seq >>  8) & 0xff;
    msg [3] = (seq >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::Ack;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg));
}

void
RtmpConnection::sendWindowAckSize (Uint32 const wack_size)
{
    Byte msg [4];

    msg [0] = (wack_size >> 24) & 0xff;
    msg [1] = (wack_size >> 16) & 0xff;
    msg [2] = (wack_size >>  8) & 0xff;
    msg [3] = (wack_size >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::WindowAckSize;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg));
}

void
RtmpConnection::sendSetPeerBandwidth (Uint32 const wack_size,
				      Byte   const limit_type)
{
    Byte msg [5];

    msg [0] = (wack_size >> 24) & 0xff;
    msg [1] = (wack_size >> 16) & 0xff;
    msg [2] = (wack_size >>  8) & 0xff;
    msg [3] = (wack_size >>  0) & 0xff;

    msg [4] = limit_type;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::SetPeerBandwidth;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg));
}

void
RtmpConnection::sendUserControl_BufferReady (Uint32 const msg_stream_id)
{
    Byte msg [6];

    msg [0] = 0x00;
    msg [1] = 0x20;

    msg [2] = (msg_stream_id >> 24) & 0xff;
    msg [3] = (msg_stream_id >> 16) & 0xff;
    msg [4] = (msg_stream_id >>  8) & 0xff;
    msg [5] = (msg_stream_id >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::UserControl;
    mdesc.msg_stream_id = msg_stream_id /* XXX CommandMessageStreamId? */;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg));
}

void
RtmpConnection::sendUserControl_StreamBegin (Uint32 const msg_stream_id)
{
    Byte msg [6];

    msg [0] = 0x00;
    msg [1] = 0x00;

    msg [2] = (msg_stream_id >> 24) & 0xff;
    msg [3] = (msg_stream_id >> 16) & 0xff;
    msg [4] = (msg_stream_id >>  8) & 0xff;
    msg [5] = (msg_stream_id >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::UserControl;
    mdesc.msg_stream_id = msg_stream_id /* XXX CommandMessageStreamId? */;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg));
}

void
RtmpConnection::sendUserControl_SetBufferLength (Uint32 const msg_stream_id,
						 Uint32 const buffer_len)
{
    Byte msg [10];

    msg [0] = 0x00;
    msg [1] = 0x03;

    msg [2] = (msg_stream_id >> 24) & 0xff;
    msg [3] = (msg_stream_id >> 16) & 0xff;
    msg [4] = (msg_stream_id >>  8) & 0xff;
    msg [5] = (msg_stream_id >>  0) & 0xff;

    msg [6] = (buffer_len >> 24) & 0xff;
    msg [7] = (buffer_len >> 16) & 0xff;
    msg [8] = (buffer_len >>  8) & 0xff;
    msg [9] = (buffer_len >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::UserControl;
    mdesc.msg_stream_id = msg_stream_id; /* XXX CommandMessageStreamId? */
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg));
}

void
RtmpConnection::sendUserControl_StreamIsRecorded (Uint32 const msg_stream_id)
{
    Byte msg [6];

    msg [0] = 0x00;
    msg [1] = 0x04;

    msg [2] = (msg_stream_id >> 24) & 0xff;
    msg [3] = (msg_stream_id >> 16) & 0xff;
    msg [4] = (msg_stream_id >>  8) & 0xff;
    msg [5] = (msg_stream_id >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::UserControl;
    mdesc.msg_stream_id = msg_stream_id; /* XXX CommandMessageStreamId? */
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg));
}

void
RtmpConnection::sendUserControl_PingRequest ()
{

    Byte msg [6];

    Uint32 const time = getTime (); // XXX Time in seconds?

    msg [0] = 0x00;
    msg [1] = 0x06;

    msg [2] = (time >> 24) & 0xff;
    msg [3] = (time >> 16) & 0xff;
    msg [4] = (time >>  8) & 0xff;
    msg [5] = (time >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::UserControl;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg));
}

void
RtmpConnection::sendUserControl_PingResponse (Uint32 const timestamp)
{
    Byte msg [6];

    msg [0] = 0x00;
    msg [1] = 0x07;

    msg [2] = (timestamp >> 24) & 0xff;
    msg [3] = (timestamp >> 16) & 0xff;
    msg [4] = (timestamp >>  8) & 0xff;
    msg [5] = (timestamp >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::UserControl;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg));
}

void
RtmpConnection::sendDataMessage_AMF0 (Uint32      const msg_stream_id,
                                      ConstMemory const mem)
{
    MessageDesc mdesc;
    mdesc.timestamp = 0; // XXX Why not set real non-zero timestamp? - Возможно, при нулевом таймстемпе сообщения не оседают в буфере, а сразу обрабатываются?
    mdesc.msg_type_id = RtmpMessageType::Data_AMF0;
    mdesc.msg_stream_id = msg_stream_id;
    mdesc.msg_len = mem.len();
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc, data_chunk_stream, mem);
}

void
RtmpConnection::sendDataMessage_AMF0_Pages (Uint32                   const msg_stream_id,
                                            PagePool::PageListHead * const mt_nonnull page_list,
                                            Size                     const msg_offset,
                                            Size                     const msg_len)
{
    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::Data_AMF0;
    mdesc.msg_stream_id = msg_stream_id;
    mdesc.msg_len = msg_len;
    mdesc.cs_hdr_comp = true;

    sendMessagePages (&mdesc,
                      data_chunk_stream,
                      page_list,
                      msg_offset,
                      false /* take_ownership */,
                      false /* unlocked */,
                      NULL  /* extra_header_buf */,
                      0     /* extra_header_len */);
}

void
RtmpConnection::sendCommandMessage_AMF0 (Uint32 const msg_stream_id,
					 ConstMemory const &mem)
{
    MessageDesc mdesc;
    mdesc.timestamp = 0; // XXX Why not set real non-zero timestamp? - Возможно, при нулевом таймстемпе сообщения не оседают в буфере, а сразу обрабатываются?
    mdesc.msg_type_id = RtmpMessageType::Command_AMF0;
    mdesc.msg_stream_id = msg_stream_id;
    mdesc.msg_len = mem.len();
    mdesc.cs_hdr_comp = false;

    sendMessage (&mdesc, data_chunk_stream, mem);
}

void
RtmpConnection::sendCommandMessage_AMF0_Pages (Uint32                   const msg_stream_id,
					       PagePool::PageListHead * const mt_nonnull page_list,
					       Size                     const msg_offset,
					       Size                     const msg_len)
{
    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::Command_AMF0;
    mdesc.msg_stream_id = msg_stream_id;
    mdesc.msg_len = msg_len;
    mdesc.cs_hdr_comp = true;

    sendMessagePages (&mdesc,
                      data_chunk_stream,
                      page_list,
                      msg_offset,
                      false /* take_ownership */,
                      false /* unlocked */,
                      NULL  /* extra_header_buf */,
                      0     /* extra_header_len */);
}

void
RtmpConnection::sendVideoMessage (VideoMessage                * const mt_nonnull msg,
                                  Sender::SenderStateCallback * const sender_state_cb,
                                  void                        * const sender_state_cb_data)
{
#if 0
    logD_ (_func_);
    logLock ();
    PagePool::dumpPages (logs, &msg->page_list);
    logUnlock ();
#endif

    // TODO Do not ignore RtmpClearMetaData.
    if (msg->frame_type == VideoFrameType::RtmpClearMetaData)
	return;

    MOMENT__RTMP_MESSAGE

    MessageDesc mdesc;
    if (!momentrtmp_proto)
        mdesc.timestamp = (Uint64) msg->pts_nanosec / 1000000;
    else
        mdesc.timestamp = (Uint64) msg->pts_nanosec / 1000;

    Byte flv_video_header [FlvVideoHeader_MaxLen];
    unsigned flv_video_header_len = 0;

    ChunkStream *chunk_stream;
    if (msg->frame_type == VideoFrameType::RtmpSetMetaData) {
	mdesc.timestamp = 0;
	mdesc.msg_type_id = RtmpMessageType::Data_AMF0;
	chunk_stream = data_chunk_stream;
    } else {
	mdesc.msg_type_id = RtmpMessageType::VideoMessage;
	chunk_stream = video_chunk_stream;

        flv_video_header_len = fillFlvVideoHeader (msg, Memory::forObject (flv_video_header));
        if (flv_video_header_len == 0 && msg->msg_len != 0) {
            logD_ (_func, "Ignoring non-empty video message: couldn't fill video header");
            return;
        }
    }

    mdesc.msg_stream_id = DefaultMessageStreamId;
    mdesc.msg_len = msg->msg_len;
    mdesc.cs_hdr_comp = true;
    mdesc.adjustable_timestamp = msg->frame_type.isVideoData();

    logS_ (_this_func, "ts ", msg->pts_nanosec, " (", mdesc.timestamp, ") ", msg->frame_type);

    sendMessagePages (&mdesc,
                      chunk_stream,
                      &msg->page_list,
                      msg->msg_offset,
                      false /* take_ownership */,
                      false /* unlocked */,
                      flv_video_header,
                      flv_video_header_len,
                      sender_state_cb,
                      sender_state_cb_data);
}

void
RtmpConnection::sendAudioMessage (AudioMessage                * const mt_nonnull msg,
                                  Sender::SenderStateCallback * const sender_state_cb,
                                  void                        * const sender_state_cb_data)
{
    // Note that nellymoser codec may generate data which makes valgrind
    // complain about uninitialized bytes.

//    logD (rtmp_server, _func_);

#if 0
    logD_ (_func_);
    logLock ();
    PagePool::dumpPages (logs, &msg->page_list);
    logUnlock ();
#endif

    Byte flv_audio_header [FlvAudioHeader_MaxLen];
    unsigned const flv_audio_header_len = fillFlvAudioHeader (msg, Memory::forObject (flv_audio_header));
    if (flv_audio_header_len == 0 && msg->msg_len != 0) {
        logD_ (_func, "Ignoring non-empty audio message: couldn't fill audio header");
        return;
    }

    MOMENT__RTMP_MESSAGE

    MessageDesc mdesc;
    if (!momentrtmp_proto)
        mdesc.timestamp = msg->pts_nanosec / 1000000;
    else
        mdesc.timestamp = msg->pts_nanosec / 1000;

    mdesc.msg_type_id = RtmpMessageType::AudioMessage;
    mdesc.msg_stream_id = DefaultMessageStreamId;
    mdesc.msg_len = msg->msg_len;
    mdesc.cs_hdr_comp = true;
    mdesc.adjustable_timestamp = msg->frame_type.isAudioData();

    sendMessagePages (&mdesc,
                      audio_chunk_stream,
                      &msg->page_list,
                      msg->msg_offset,
                      false /* take_ownership */,
                      false /* unlocked */,
                      flv_audio_header,
                      flv_audio_header_len,
                      sender_state_cb,
                      sender_state_cb_data);
}

void
RtmpConnection::sendConnect (ConstMemory const app_name,
                             ConstMemory const page_url,
                             ConstMemory const swf_url,
                             ConstMemory const tc_url)
{
    AmfAtom atoms [26];
    AmfEncoder encoder (atoms);

    encoder.addString ("connect");
    encoder.addNumber (1.0);

    {
	encoder.beginObject ();

	encoder.addFieldName ("app");
	encoder.addString (app_name);

	encoder.addFieldName ("flashVer");
	encoder.addString ("LNX 10,0,22,87");

	encoder.addFieldName ("swfUrl");
	encoder.addString (swf_url);

	encoder.addFieldName ("tcUrl");
	encoder.addString (tc_url);

	encoder.addFieldName ("pageUrl");
	encoder.addString (page_url);

	encoder.addFieldName ("fpad");
	encoder.addBoolean (false);

	encoder.addFieldName ("capabilities");
	encoder.addNumber (15.0);

	encoder.addFieldName ("audioCodecs");
	encoder.addNumber ((double) 0x00fff /* SUPPORT_SND_ALL */);

	encoder.addFieldName ("videoCodecs");
	encoder.addNumber ((double) 0x00ff /* SUPPORT_VID_ALL */);

	encoder.addFieldName ("videoFunction");
	encoder.addNumber (1.0);

	encoder.addFieldName ("objectEncoding");
	encoder.addNumber (0.0);

	encoder.endObject ();
    }

    Byte msg_buf [1024];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	logE_ (_func, "encode() failed");
	unreachable ();
    }

    sendCommandMessage_AMF0 (CommandMessageStreamId, ConstMemory (msg_buf, msg_len));
    logD (send, _func, "msg_len: ", msg_len);
    logHex_ (LogLevel::Debug, ConstMemory (msg_buf, msg_len), _func);
}

void
RtmpConnection::sendCreateStream ()
{
    AmfAtom atoms [3];
    AmfEncoder encoder (atoms);

    encoder.addString ("createStream");
    // FIXME Use saner transaction ids.
    encoder.addNumber (2.0);

    encoder.addNullObject ();

    Byte msg_buf [512];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	logE_ (_func, "encode() failed");
	unreachable ();
    }

    sendCommandMessage_AMF0 (CommandMessageStreamId, ConstMemory (msg_buf, msg_len));
}

void
RtmpConnection::sendPlay (ConstMemory const stream_name)
{
    AmfAtom atoms [4];
    AmfEncoder encoder (atoms);

    encoder.addString ("play");
    // FIXME Use saner transaction ids.
    encoder.addNumber (3.0);
    encoder.addNullObject ();
    encoder.addString (stream_name);

    // FIXME stream_name shouldn't be too long, otherwise the message will not be sent.
    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	logE_ (_func, "encode() failed");
	// TODO Uncomment once message length limitation is resolved.
	// unreachable ();
    }

//    sendCommandMessage_AMF0 (CommandMessageStreamId, ConstMemory (msg_buf, msg_len));
    sendCommandMessage_AMF0 (1, ConstMemory (msg_buf, msg_len));
}

void
RtmpConnection::sendPublish (ConstMemory const stream_name,
                             ConstMemory const  record_mode)
{
    AmfAtom atoms [5];
    AmfEncoder encoder (atoms);

    encoder.addString ("publish");
    // FIXME Use saner transaction ids.
    encoder.addNumber (4.0);
    encoder.addNullObject ();
    encoder.addString (stream_name);
    encoder.addString (record_mode);

    // FIXME stream_name shouldn't be too long, otherwise the message will not be sent.
    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	logE_ (_func, "encode() failed");
	// TODO Uncomment once message length limitation is resolved.
	// unreachable ();
    }

    sendCommandMessage_AMF0 (1, ConstMemory (msg_buf, msg_len));
}

void
RtmpConnection::closeAfterFlush ()
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) this);
    sender->flush ();
    sender->closeAfterFlush ();
}

void
RtmpConnection::close ()
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) this);
    frontend.call (frontend->closed, /*(*/ (Exception*) NULL /*)*/);
    backend.call (backend->close, DisconnectReason::LocalClose);
}

void
RtmpConnection::close_noBackendCb ()
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) this);
    frontend.call (frontend->closed, /*(*/ (Exception*) NULL /*)*/);
}

mt_const void
RtmpConnection::beginPings ()
{
    assert (!ping_send_timer);
    if (ping_timeout_millisec != 0) {
        ping_send_timer =
                timers->addTimer_microseconds (CbDesc<Timers::TimerCallback> (pingTimerTick, this, this),
                                               ping_timeout_millisec * 1000,
                                               true  /* periodical */,
                                               false /* auto_delete */);
    }
}

void
RtmpConnection::pingTimerTick (void * const _self)
{
    RtmpConnection * const self = static_cast <RtmpConnection*> (_self);

    if (!self->ping_reply_received.compareAndExchange (1 /* old_value */, 0 /* new_value */)) {
      // 'self->ping_reply_received' if 'false'.

        {
            logLock ();
            logE_locked_ (_self_func, "no ping reply");
            if (int const hit = self->ping_timeout_expired_once.fetchAdd (1)) {
                logE_locked_ (_self_func, "pingTimerTick hit #", hit);
            }
            logUnlock ();
        }

        self->in_destr_mutex.lock ();
        if (self->ping_send_timer) {
            self->timers->deleteTimer (self->ping_send_timer);
            self->ping_send_timer = NULL;
        }
        self->in_destr_mutex.unlock ();

	logD (close, _self_func, "closing");
	{
	    InternalException internal_exc (InternalException::ProtocolError);
	    self->frontend.call (self->frontend->closed, /*(*/ &internal_exc /*)*/);
	}
	self->backend.call (self->backend->close, DisconnectReason::CommunicationTimeout);
	return;
    }

//#warning TODO Send ping request only after handshake is complete.
    self->sendUserControl_PingRequest ();
}

mt_sync_domain (receiver) Result
RtmpConnection::processMessage (ChunkStream * const chunk_stream)
{
    logD (msg, _func_);

    logD (proto_in, _func, "ts 0x", fmt_hex, chunk_stream->in_msg_timestamp,
	  " (", fmt_def, chunk_stream->in_msg_timestamp, "), "
	  "tid ", fmt_def, chunk_stream->in_msg_type_id, " (", (RtmpMessageType) chunk_stream->in_msg_type_id, ")"
	  ", msid ", chunk_stream->in_msg_stream_id, ", csid ", chunk_stream->chunk_stream_id,
	  ", mlen ", chunk_stream->in_msg_len);

    if (logLevelOn (proto_in, LogLevel::Stream))
        PagePool::logPages (LogLevel::Stream, chunk_stream->page_list.first, 0 /* start_page_offset */, _func);

    if (dump_stream) {
        dump_stream->println ("msg ts ", chunk_stream->in_msg_timestamp, " "
                              "type ", chunk_stream->in_msg_type_id, " (", (RtmpMessageType) chunk_stream->in_msg_type_id, ") "
                              "msid ", chunk_stream->in_msg_stream_id, " "
                              "csid ", chunk_stream->chunk_stream_id, " "
                              "mlen ", chunk_stream->in_msg_len);
    }

    Byte const *msg_buf = chunk_stream->page_list.first->getData();
    Size const msg_len = chunk_stream->in_msg_len;

    logD (msg, _func, "message type id: ", chunk_stream->in_msg_type_id);
    switch (chunk_stream->in_msg_type_id) {
	case RtmpMessageType::SetChunkSize: {
	    logD (proto_in, _func, "SetChunkSize");

	    if (msg_len < 4) {
		logE_ (_func, "SetChunkSize message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const chunk_size = ((Uint32) msg_buf [0] << 24) |
				      ((Uint32) msg_buf [1] << 16) |
				      ((Uint32) msg_buf [2] <<  8) |
				      ((Uint32) msg_buf [3] <<  0);

            if (dump_stream)
                dump_stream->println ("SetChunkSize chunk_size ", chunk_size);

            /* This check is pointless.
             * We can handle any chunk size just fine.
             *
	    if (chunk_size < MinChunkSize ||
		chunk_size > MaxChunkSize)
	    {
		logE_ (_func, "SetChunkSize: bad chunk size: ", chunk_size);
		return Result::Failure;
	    }
            */

	    logD (proto_in, _func, "SetChunkSize: new chunk size: ", chunk_size);

	    in_chunk_size = chunk_size;
	} break;
	case RtmpMessageType::Abort: {
	    // TODO Verify that Abort is handled correctly.

	    logD (proto_in, _func, "Abort");

	    if (msg_len < 4) {
		logE_ (_func, "Abort message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const chunk_stream_id = ((Uint32) msg_buf [0] << 24) |
					   ((Uint32) msg_buf [1] << 16) |
					   ((Uint32) msg_buf [2] <<  8) |
					   ((Uint32) msg_buf [3] <<  0);

	    logD (proto_in, _func, "Abort: chunk_stream_id: ", chunk_stream_id);

            if (dump_stream)
                dump_stream->println ("Abort cs_id ", chunk_stream_id);

	    ChunkStream * const chunk_stream = getChunkStream (chunk_stream_id, false /* create */);
	    if (!chunk_stream) {
		logW_ (_func, "Abort: stream not found: ", chunk_stream_id);
		return Result::Success;
	    }

	    resetMessageRecvState (chunk_stream);
	} break; 
	case RtmpMessageType::Ack: {
	    logD (proto_in, _func, "Ack");

	    if (msg_len < 4) {
		logE_ (_func, "Ack message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const bytes_received = ((Uint32) msg_buf [0] << 24) |
					  ((Uint32) msg_buf [1] << 16) |
					  ((Uint32) msg_buf [2] <<  8) |
					  ((Uint32) msg_buf [3] <<  0);

	    logD (proto_in, _func, "Ack: bytes_received: ", bytes_received);

            if (dump_stream)
                dump_stream->println ("Ack bytes_received ", bytes_received);

	    // TODO Handle acks.
	} break;
	case RtmpMessageType::UserControl: {
	    logD (proto_in, _func, "UserControl");

	    return processUserControlMessage (chunk_stream);
	} break;
	case RtmpMessageType::WindowAckSize: {
	    logD (proto_in, _func, "WindowAckSize");

	    if (msg_len < 4) {
		logE_ (_func, "WindowAckSize message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const wack_size = ((Uint32) msg_buf [0] << 24) |
				     ((Uint32) msg_buf [1] << 16) |
				     ((Uint32) msg_buf [2] <<  8) |
				     ((Uint32) msg_buf [3] <<  0);

	    logD (proto_in, _func, "WindowAckSize: wack_size: ", wack_size);

            if (dump_stream)
                dump_stream->println ("WindowAckSize wack_size ", wack_size);

	    remote_wack_size = wack_size;
	} break;
	case RtmpMessageType::SetPeerBandwidth: {
	    logD (proto_in, _func, "SetPeerBandwidth");

	    if (msg_len < 5) {
		logE_ (_func, "SetPeerBandwidth message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const wack_size = ((Uint32) msg_buf [0] << 24) |
				     ((Uint32) msg_buf [1] << 16) |
				     ((Uint32) msg_buf [2] <<  8) |
				     ((Uint32) msg_buf [3] <<  0);

	    logD (proto_in, _func, "SetPeerBandwidth: wack_size: ", wack_size, ", limit_type: ", (Uint32) msg_buf [4]);

            if (dump_stream)
                dump_stream->println ("SetPeerBandwidth wack_size ", wack_size);

	    // Unused
	    // Byte const limit_type = msg_buf [4];

	    // Note: SetPeerBandwidth message does nothing. This doesn't feel right.

	    if (local_wack_size != wack_size)
		sendWindowAckSize (local_wack_size);
	} break;
	case RtmpMessageType::AudioMessage: {
	    logD (proto_in, _func, "AudioMessage");

	    if (frontend && frontend->audioMessage) {
		AudioMessage audio_msg;

                unsigned flv_audio_header_len = 0;
		{
		    PagePool::Page * const page = chunk_stream->page_list.first;
		    if (page && page->data_len >= 1) {
//			logD (proto_in, "audio header: 0x", fmt_hex, (unsigned) page->getData() [0]);

			Byte const codec_id = (page->getData() [0] & 0xf0) >> 4;
                        unsigned const rate = flvSamplingRateToNumeric ((page->getData() [0] & 0x0c) >> 2);
                        unsigned const channels = (page->getData() [0] & 1) + 1;

			audio_msg.codec_id = AudioCodecId::fromFlvCodecId (codec_id);
			audio_msg.frame_type = AudioFrameType::RawData;
                        audio_msg.rate = rate;
                        audio_msg.channels = channels;

                        flv_audio_header_len = 1;
			if (audio_msg.codec_id == AudioCodecId::AAC) {
			    if (page->data_len >= 2) {
				if (page->getData() [1] == 0)
				    audio_msg.frame_type = AudioFrameType::AacSequenceHeader;

                                flv_audio_header_len = 2;
			    }
                        }
		    } else {
                        // Flash sends empty audio messages when going silent with Speex.
			audio_msg.frame_type = AudioFrameType::Unknown;
			audio_msg.codec_id = AudioCodecId::Unknown;
		    }
		}

                if (!momentrtmp_proto) {
                    audio_msg.pts_nanosec = (Uint64) chunk_stream->in_msg_timestamp * 1000000;
                    audio_msg.dts_nanosec = audio_msg.pts_nanosec;
                } else {
                    // TODO Send DTS in momentrtmp_proto
                    audio_msg.pts_nanosec = chunk_stream->in_msg_timestamp * 1000;
                    audio_msg.dts_nanosec = audio_msg.pts_nanosec;
                }

		audio_msg.page_pool = page_pool;
		audio_msg.page_list = chunk_stream->page_list;

                // Cutting away FLV audio packet header.
                assert (flv_audio_header_len <= msg_len);
		audio_msg.msg_len = msg_len - flv_audio_header_len;
		audio_msg.msg_offset = flv_audio_header_len;

		Result res = Result::Failure;
		frontend.call_ret<Result> (&res, frontend->audioMessage, /*(*/ &audio_msg /*)*/);
		return res;
	    }
	} break;
	case RtmpMessageType::VideoMessage: {
	    logD (proto_in, _func, "VideoMessage");

	    if (frontend && frontend->videoMessage) {
		VideoMessage video_msg;

                unsigned flv_video_header_len = 0;
		{
		    PagePool::Page * const page = chunk_stream->page_list.first;
		    if (page && page->data_len >= 1) {
			Byte const frame_type = (page->getData() [0] & 0xf0) >> 4;
			Byte const codec_id = page->getData() [0] & 0x0f;

			video_msg.frame_type = VideoFrameType::fromFlvFrameType (frame_type);
			video_msg.codec_id = VideoCodecId::fromFlvCodecId (codec_id);

                        flv_video_header_len = 1;
			if (video_msg.codec_id == VideoCodecId::AVC) {
			    if (page->data_len >= 2) {
				if (page->getData() [1] == 0)
				    video_msg.frame_type = VideoFrameType::AvcSequenceHeader;
				else
				if (page->getData() [1] == 2)
				    video_msg.frame_type = VideoFrameType::AvcEndOfSequence;
			    }

                            if (page->data_len >= 5)
                                flv_video_header_len = 5;
                        }
		    } else {
			video_msg.frame_type = VideoFrameType::Unknown;
			video_msg.codec_id = VideoCodecId::Unknown;
		    }
		}

		logD (codec, _func, video_msg.codec_id, ", ", video_msg.frame_type);

                if (!momentrtmp_proto) {
                    video_msg.pts_nanosec = (Uint64) chunk_stream->in_msg_timestamp * 1000000;
                    video_msg.dts_nanosec = video_msg.pts_nanosec;
                } else {
                    // TODO Send DTS in momentrtmp_proto
                    video_msg.pts_nanosec = (Uint64) chunk_stream->in_msg_timestamp * 1000;
                    video_msg.dts_nanosec = video_msg.pts_nanosec;
                }

		video_msg.page_pool = page_pool;
		video_msg.page_list = chunk_stream->page_list;

                // Cutting away FLV video packet header.
                assert (flv_video_header_len <= msg_len);
		video_msg.msg_len = msg_len - flv_video_header_len;
		video_msg.msg_offset = flv_video_header_len;

		Result res = Result::Failure;
		frontend.call_ret<Result> (&res, frontend->videoMessage, /*(*/ &video_msg /*)*/);
		return res;
	    }
	} break;
	case RtmpMessageType::Data_AMF3: {
	    logD (proto_in, _func, "Data_AMF3");
	    return callCommandMessage (chunk_stream, AmfEncoding::AMF3, true /* is_data */);
	} break;
	case RtmpMessageType::Data_AMF0: {
	    logD (proto_in, _func, "Data_AMF0");
	    return callCommandMessage (chunk_stream, AmfEncoding::AMF0, true /* is_data */);
	} break;
	case RtmpMessageType::SharedObject_AMF3: {
	    logD (proto_in, _func, "SharedObject_AMF3");
	  // No-op
	} break;
	case RtmpMessageType::SharedObject_AMF0: {
	    logD (proto_in, _func, "SharedObject_AMF0");
	  // No-op
	} break;
	case RtmpMessageType::Command_AMF3: {
	    logD (proto_in, _func, "Command_AMF3");
	    return callCommandMessage (chunk_stream, AmfEncoding::AMF3, false /* is_data */);
	} break;
	case RtmpMessageType::Command_AMF0: {
	    logD (proto_in, _func, "Command_AMF0");
	    return callCommandMessage (chunk_stream, AmfEncoding::AMF0, false /* is_data */);
	} break;
	case RtmpMessageType::Aggregate: {
	    logD (proto_in, _func, "Aggregate");
	  // No-op
	} break;
	default: {
            logLock ();
	    logW_locked_ (_func, "unknown message type: 0x", fmt_hex, chunk_stream->in_msg_type_id);
            if (logLevelOn (proto_in, LogLevel::Stream)) {
                PagePool::logPages (LogLevel::Stream, chunk_stream->page_list.first, 0 /* first_page_offset */, _func);
            }
            logUnlock ();
        }
    }

    return Result::Success;
}

mt_sync_domain (receiver) Result
RtmpConnection::callCommandMessage (ChunkStream * const chunk_stream,
				    AmfEncoding   const amf_encoding,
                                    bool          const is_data)
{
    if (logLevelOn (proto_in, LogLevel::Debug)) {
	if (chunk_stream->page_list.first) {
	    logD (proto_in, _func_);
            logHex_ (LogLevel::Debug,
                     ConstMemory (chunk_stream->page_list.first->getData(),
                                  chunk_stream->page_list.first->data_len),
                     _func);
	}
    }

    if (frontend && frontend->commandMessage) {
	MediaMessage msg;
        if (!momentrtmp_proto) {
            msg.pts_nanosec = (Uint64) chunk_stream->in_msg_timestamp * 1000000;
            msg.dts_nanosec = msg.pts_nanosec;
        } else {
            // TODO Send DTS in momentrtmp_proto
            msg.pts_nanosec = (Uint64) chunk_stream->in_msg_timestamp * 1000;
            msg.dts_nanosec = msg.pts_nanosec;
        }

	msg.page_pool = page_pool;
	msg.page_list = chunk_stream->page_list;
	msg.msg_len = chunk_stream->in_msg_len;
	msg.msg_offset = 0;

        ConnectionInfo conn_info;
        conn_info.momentrtmp_proto = momentrtmp_proto;

	Result res = Result::Failure;
	frontend.call_ret<Result> (&res, frontend->commandMessage, /*(*/
		is_data, &msg, chunk_stream->in_msg_stream_id, amf_encoding, &conn_info /*)*/);
	return res;
    }

    return Result::Success;
}

mt_sync_domain (receiver) Result
RtmpConnection::processUserControlMessage (ChunkStream * const chunk_stream)
{
    Byte const *msg_buf = chunk_stream->page_list.first->getData();
    Size const msg_len = chunk_stream->in_msg_len;

    Uint32 const uc_type = ((Uint32) msg_buf [0] << 8) |
			   ((Uint32) msg_buf [1] << 0);
    switch (uc_type) {
	case UserControlMessageType::StreamBegin: {
            if (msg_len < 6) {
                logD_ (_func, "StreamBegin message is too short (", msg_len, " bytes)");
                return Result::Success;
            }

	    Uint32 const msg_stream_id = ((Uint32) msg_buf [2] << 24) |
                                         ((Uint32) msg_buf [3] << 16) |
                                         ((Uint32) msg_buf [4] <<  8) |
                                         ((Uint32) msg_buf [5] <<  0);
	    logD (proto_in, _func, "StreamBegin: msg_stream_id ", msg_stream_id);

            if (dump_stream)
                dump_stream->println ("StreamBegin msg_stream_id ", msg_stream_id);

	  // No-op
	} break;
	case UserControlMessageType::StreamEof: {
	    logD (proto_in, _func, "StreamEof");
	  // No-op
	} break;
	case UserControlMessageType::StreamDry: {
	    logD (proto_in, _func, "StreamDry");
	  // No-op
	} break;
	case UserControlMessageType::SetBufferLength: {
            if (msg_len < 10) {
                logD_ (_func, "SetBufferLength message is too short (", msg_len, " bytes)");
                return Result::Success;
            }

	    Uint32 const msg_stream_id = ((Uint32) msg_buf [2] << 24) |
                                         ((Uint32) msg_buf [3] << 16) |
                                         ((Uint32) msg_buf [4] <<  8) |
                                         ((Uint32) msg_buf [5] <<  0);

	    Uint32 const buffer_len = ((Uint32) msg_buf [6] << 24) |
                                      ((Uint32) msg_buf [7] << 16) |
                                      ((Uint32) msg_buf [8] <<  8) |
                                      ((Uint32) msg_buf [9] <<  0);

	    logD (proto_in, _func, "SetBufferLength: msg_stream_id ", msg_stream_id, " buffer_len ", buffer_len);

            if (dump_stream)
                dump_stream->println ("SetBufferLength msg_stream_id ", msg_stream_id, " buffer_len ", buffer_len);

	  // No-op
	} break;
	case UserControlMessageType::StreamIsRecorded: {
	    logD (proto_in, _func, "StreamIsRecorded");
	  // No-op
	} break;
	case UserControlMessageType::PingRequest: {
	    logD (proto_in, _func, "PingRequest");

	    if (msg_len < 6) {
		logE_ (_func, "PingRequest message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const timestamp = ((Uint32) msg_buf [2] << 24) |
				     ((Uint32) msg_buf [3] << 16) |
				     ((Uint32) msg_buf [1] <<  8) |
				     ((Uint32) msg_buf [0] <<  0);

            if (dump_stream)
                dump_stream->println ("PingRequest timestamp ", timestamp);

	    sendUserControl_PingResponse (timestamp);
	} break;
	case UserControlMessageType::PingResponse: {
	    logD (proto_in, _func, "PingResponse");

	    ping_reply_received.set (1);
	} break;
        case UserControlMessageType::BufferEmpty: {
            if (logLevelOn (proto_in, LogLevel::Debug)) {
                logD (proto_in, _func, "BufferEmpty");
                logHex_ (LogLevel::Debug, ConstMemory (msg_buf, msg_len), _func);
            }
          // No-op
        } break;
        case UserControlMessageType::BufferReady: {
            if (logLevelOn (proto_in, LogLevel::Debug)) {
                logD (proto_in, _func, "BufferReady");
                logHex_ (LogLevel::Debug, ConstMemory (msg_buf, msg_len), _func);
            }
          // No-op
        } break;
	default:
	    logD_ (_func, "unknown message type: ", uc_type);
    }

    return Result::Success;
}

void
RtmpConnection::senderStateChanged (SenderState   const sender_state,
				    void        * const _self)
{
    RtmpConnection * const self = static_cast <RtmpConnection*> (_self);
    self->frontend.call (self->frontend->senderStateChanged, /*(*/ sender_state /*)*/);
}

void
RtmpConnection::senderClosed (Exception * const exc_,
			      void      * const _self)
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) _self);

    RtmpConnection * const self = static_cast <RtmpConnection*> (_self);
    self->frontend.call (self->frontend->closed, exc_);
    self->backend.call (self->backend->close, DisconnectReason::TodoCategorizeMe);
}

mt_sync_domain (receiver) Receiver::ProcessInputResult
RtmpConnection::processInput (Memory   const mem,
			      Size   * const mt_nonnull ret_accepted,
			      void   * const _self)
{
    RtmpConnection * const self = static_cast <RtmpConnection*> (_self);
    return self->doProcessInput (mem, ret_accepted);
}

static Byte glob_fms_key [68] = {
    0x47, 0x65, 0x6e, 0x75,  0x69, 0x6e, 0x65, 0x20,
    0x41, 0x64, 0x6f, 0x62,  0x65, 0x20, 0x46, 0x6c,
    0x61, 0x73, 0x68, 0x20,  0x4d, 0x65, 0x64, 0x69,
    0x61, 0x20, 0x53, 0x65,  0x72, 0x76, 0x65, 0x72,
    0x20, 0x30, 0x30, 0x31,  0xf0, 0xee, 0xc2, 0x4a,
    0x80, 0x68, 0xbe, 0xe8,  0x2e, 0x00, 0xd0, 0xd1,
    0x02, 0x9e, 0x7e, 0x57,  0x6e, 0xec, 0x5d, 0x2d,
    0x29, 0x80, 0x6f, 0xab,  0x93, 0xb8, 0xe6, 0x36,
    0xcf, 0xeb, 0x31, 0xae
};

static Uint32
getDigestOffset (Byte const * const msg,
		 int          const handshake_scheme)
{
    Uint32 server_digest_offs = 0;
    switch (handshake_scheme) {
	case 0:
	    server_digest_offs = (Uint32) msg [ 8] +
				 (Uint32) msg [ 9] +
				 (Uint32) msg [10] +
				 (Uint32) msg [11];
	    server_digest_offs %= 728;
	    server_digest_offs += 12;
	    break;
	case 1:
	    server_digest_offs = (Uint32) msg [772] +
				 (Uint32) msg [773] +
				 (Uint32) msg [774] +
				 (Uint32) msg [775];
	    server_digest_offs %= 728;
	    server_digest_offs += 776;
	    break;
	default:
	    unreachable ();
    }

    return server_digest_offs;
}

mt_sync_domain (receiver) Receiver::ProcessInputResult
RtmpConnection::doProcessInput (ConstMemory   const mem,
				Size        * const mt_nonnull ret_accepted)
{
    logD (msg, _func, "mem.len(): ", mem.len());

    *ret_accepted = 0;

    if (mem.len() == 0)
	return Receiver::ProcessInputResult::Again;

    processing_input = true;

    Byte const *data = mem.mem();
    Size len = mem.len();
    total_received += len;

    if (// Send acks only after something has been received actually (to avoid ack storms).
	remote_wack_size >= 2 &&
	// Sending acks twice as often as needed for extra safety.
	total_received - last_ack >= remote_wack_size / 2)
    {
	last_ack = total_received;
	sendAck (total_received /* seq */);
    }

    Receiver::ProcessInputResult ret_res = Receiver::ProcessInputResult::Normal;

    for (;;) {
	switch (conn_state.val()) {
	    case ReceiveState::Invalid:
		unreachable ();
	    case ReceiveState::ClientWaitS0: {
		if (len < 1) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}

		Byte const server_version = data [0];
		if (server_version < 3) {
		  // Deprecated protocols.
		    logE_ (_func, "ClientWaitS0: old protocol version: ", server_version);
		}

                if (dump_stream)
                    dump_stream->println ("handshake S0 server_version ", (Uint32) server_version);

		{
		    data += 1;
		    len -= 1;
		}

		conn_state = ReceiveState::ClientWaitS1;
	    } break;
	    case ReceiveState::ClientWaitS1: {
		if (len < 1536) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}

                if (dump_stream)
                    dump_stream->println ("handshake S1 csum ", calculateChecksum (ConstMemory (data, 1536)));

		{
		    PagePool::PageListInfo page_list;
		    page_pool->getPages (&page_list, 1536 /* len */);
		    assert (page_list.first->data_len >= 1536);
		    Byte * const msg_c2 = page_list.first->getData();
		    memcpy (msg_c2, data, 4);
		    {
			Uint32 const time = getTimeMilliseconds ();
			msg_c2 [4] = (time >>  0) & 0xff;
			msg_c2 [5] = (time >>  8) & 0xff;
			msg_c2 [6] = (time >> 16) & 0xff;
			msg_c2 [7] = (time >> 24) & 0xff;
		    }

		    memcpy (msg_c2 + 8, data, 1536 - 8);
		    sendRawPages (page_list.first, page_list.data_len);
		}

		{
		    data += 1536;
		    len -= 1536;
		}

		conn_state = ReceiveState::ClientWaitS2;
	    } break;
	    case ReceiveState::ClientWaitS2: {
		if (len < 1536) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}

                if (dump_stream)
                    dump_stream->println ("handshake S2 csum ", calculateChecksum (ConstMemory (data, 1536)));

		{
		    data += 1536;
		    len -= 1536;
		}

		if (frontend && frontend->handshakeComplete) {
		    Result res;
		    if (!frontend.call_ret<Result> (&res, frontend->handshakeComplete)) {
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }

		    if (!res) {
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }
		}

		conn_state = ReceiveState::BasicHeader;
	    } break;
	    case ReceiveState::ServerWaitC0: {
		logD (msg, _func, "ServerWaitC0");

		if (len < 1) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}

		Byte const client_version = data [0];
                if (client_version == 0xff) {
                    logD_ (_func, "momentrtmp proto");
                    if (!momentrtmp_proto) {
                        send_mutex.lock ();
                        momentrtmp_proto = true;
                        send_mutex.unlock ();
                    }
                } else
                if (client_version < 3) {
                  // Deprecated protocols.
                    logE_ (_func, "ServerWaitC0: old protocol version: ", client_version);
                    ret_res = Receiver::ProcessInputResult::Error;
                    goto _return;
                }

                if (dump_stream)
                    dump_stream->println ("handshake C0 clinet_version ", (Uint32) client_version);

		{
		    data += 1;
		    len -= 1;
		}

		{
		  // Sending S0

                    logD (proto_out, _func, "Sending S0");

		    SenderMessageEntry_Pages * const msg_pages =
			    SenderMessageEntry_Pages::createNew (1);
                    msg_pages->init (/*first_page=*/ NULL, /*page_pool=*/ NULL, /*msg_offs=*/ 0, /*msg_len=*/ 0);

		    msg_pages->getHeaderData() [0] = 3;
		    msg_pages->header_len = 1;

		    sender->sendMessage (msg_pages, true /* do_flush */);
		}

#if 0
// Old handshake code.
		{
		    PagePool::PageListHead page_list;
		    page_pool->getPages (&page_list, 1537 /* len */);
		    assert (page_list.first->data_len >= 1537);
		    Byte * const msg_s1 = page_list.first->getData();
		    msg_s1 [0] = 3;
		    {
			Uint32 const time = getTimeMilliseconds ();
			msg_s1 [1] = (time >>  0) & 0xff;
			msg_s1 [2] = (time >>  8) & 0xff;
			msg_s1 [3] = (time >> 16) & 0xff;
			msg_s1 [4] = (time >> 24) & 0xff;
		    }
		    memset (msg_s1 + 5, 0 , 4);

		    {
			unsigned n = 0;
			for (unsigned i = 9; i < 1537; ++i) {
			    n = (1536 + i + n) % 317;
			    msg_s1 [i] = n;
			}
		    }

		    sendRawPages (page_list.first);
		}
#endif

		conn_state = ReceiveState::ServerWaitC1;
	    } break;
	    case ReceiveState::ServerWaitC1: {
		logD (msg, _func, "ServerWaitC1");

		if (len < 1536) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}

                if (dump_stream)
                    dump_stream->println ("handshake C1 csum ", calculateChecksum (ConstMemory (data, 1536)));

		{
		    PagePool::PageListInfo page_list;
		    page_pool->getPages (&page_list, 3072);
		    // We want pages to be large enough to hold both S1 and S2,
		    // just because it is easier to code things this way.
		    // This is likely the largest of such page size requirements.
		    assert (page_list.first->data_len >= 3072);
		    Byte * const msg = page_list.first->getData();

		    Uint32 const client_version = (data [4] << 24) |
						  (data [5] << 16) |
						  (data [6] <<  8) |
						  (data [7] <<  0);

		    // 0 - for players before 10.0.32.18
		    // 1 - for newer players
		    int handshake_scheme = 0;
		    if (client_version >= 0x80000302)
			handshake_scheme = 1;

		    {
			Uint32 const time = getTimeMilliseconds ();
			msg [0] = (time >>  0) & 0xff;
			msg [1] = (time >>  8) & 0xff;
			msg [2] = (time >> 16) & 0xff;
			msg [3] = (time >> 24) & 0xff;
		    }

		    {
			msg [4] = 3;
			msg [5] = 0;
			msg [6] = 2;
			msg [7] = 1;
		    }

		    {
		      // Generating some "random" data.
			unsigned n = randomUint32() % 3072;
			for (unsigned i = 8; i < 3072 - 8; ++i) {
			    n = (3072 + i + n) % 317;
			    msg [i] = n;
			}
		    }

		    {
		      // Computing 32-byte validation code for S1.

			Uint32 const server_digest_offs = getDigestOffset (msg, handshake_scheme);

			Byte server_hash_buf [1536 - 32];
			memcpy (server_hash_buf, msg, server_digest_offs);
			memcpy (server_hash_buf + server_digest_offs,
				msg + server_digest_offs + 32,
				sizeof (server_hash_buf) - server_digest_offs);

			hmac_sha256 (glob_fms_key, 36,
				     server_hash_buf, sizeof (server_hash_buf),
				     msg + server_digest_offs, 32);
		    }

		    {
		      // Computing 32-byte validation code for S2.

			Uint32 const client_digest_offs = getDigestOffset (data, handshake_scheme);

			Byte hash_key [32];
			hmac_sha256 (glob_fms_key, sizeof (glob_fms_key),
				     const_cast <unsigned char*> (data + client_digest_offs), 32,
				     hash_key, sizeof (hash_key));

			hmac_sha256 (hash_key, sizeof (hash_key),
				     msg + 1536, 1536 - 32,
				     msg + (1536 * 2 - 32), sizeof (hash_key));
		    }

                    logD (proto_out, _func, "Sending S1+S2");

		    sendRawPages (page_list.first, page_list.data_len);
		}


#if 0
// Old handshake code.
		{
		    PagePool::PageListHead page_list;
		    page_pool->getPages (&page_list, 1536 /* len */);
		    assert (page_list.first->data_len >= 1536);
		    Byte * const msg_s2 = page_list.first->getData();
		    memcpy (msg_s2, data, 4);
		    memset (msg_s2 + 4, 0, 4);

		    memcpy (msg_s2 + 8, data, 1536 - 8);
		    sendRawPages (page_list.first);
		}
#endif

		{
		    data += 1536;
		    len -= 1536;
		}

		conn_state = ReceiveState::ServerWaitC2;
	    } break;
	    case ReceiveState::ServerWaitC2: {
		logD (msg, _func, "ServerWaitC2");

		if (len < 1536) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}

                if (dump_stream)
                    dump_stream->println ("handshake C2 csum ", calculateChecksum (ConstMemory (data, 1536)));

		{
		    data += 1536;
		    len -= 1536;
		}

		if (frontend && frontend->handshakeComplete) {
		    Result res;
		    if (!frontend.call_ret<Result> (&res, frontend->handshakeComplete)) {
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }

		    if (!res) {
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }
		}

		conn_state = ReceiveState::BasicHeader;
	    } break;
	    case ReceiveState::BasicHeader: {
		logD (msg, _func, "BasicHeader");

		if (len < 1) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    logD (chunk, _func, "len < 1, returning Again");
		    goto _return;
		}

		bool next_state = false;
		switch (cs_id__fmt.val()) {
		    case CsIdFormat::Unknown: {
			fmt = (data [0] & 0xc0) >> 6;

			unsigned const local_cs_id = data [0] & 0x3f;
			switch (local_cs_id) {
			    case 0: {
			      // Ids 64-319
				cs_id = 64;
				cs_id__fmt = CsIdFormat::OneByte;
				logD (chunk, _func, "expecting CsIdFormat::OneByte");
			    } break;
			    case 1: {
			      // Ids 64-65536
				cs_id = 64;
				cs_id__fmt = CsIdFormat::TwoBytes_First;
				logD (chunk, _func, "expecting CsIdFormat::TwoBytes_First");
			    } break;
			    case 2: {
			      // Low-level protocol message
				cs_id = local_cs_id;
				next_state = true;
			    } break;
			    default: {
			      // Ids 3-63
				cs_id = local_cs_id;
				next_state = true;
			    }
			}
		    } break;
		    case CsIdFormat::OneByte: {
			cs_id += data [0];
			next_state = true;
		    } break;
		    case CsIdFormat::TwoBytes_First: {
			cs_id += data [0];
			cs_id__fmt = CsIdFormat::TwoBytes_Second;
			logD (chunk, _func, "expecting CsIdFormat::TwoBytes_Second");
		    } break;
		    case CsIdFormat::TwoBytes_Second: {
			cs_id += ((Uint16) data [0]) << 8;
			next_state = true;
		    } break;
		    default:
			unreachable ();
		}

		{
		    data += 1;
		    len -= 1;
		}

		if (next_state) {
		    logD (msg, _func, "chunk stream id: ", cs_id);

		    recv_chunk_stream = getChunkStream (cs_id, true /* create */);
		    logD (msg, _func, "recv_chunk_stream: 0x", fmt_hex, (UintPtr) recv_chunk_stream);
		    if (!recv_chunk_stream) {
			logE_ (_func, "stream not found: ", cs_id);
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }

		    switch (fmt) {
			case 0:
			    conn_state = ReceiveState::ChunkHeader_Type0;
			    break;
			case 1:
			    conn_state = ReceiveState::ChunkHeader_Type1;
			    break;
			case 2:
			    conn_state = ReceiveState::ChunkHeader_Type2;
			    break;
			case 3:
			    conn_state = ReceiveState::ChunkHeader_Type3;
			    break;
			default:
			    unreachable ();
		    }
		}
	    } break;
	    case ReceiveState::ChunkHeader_Type0: {
		logD (msg, _func, "ChunkHeader_Type0");

		if (len < 11) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}

		Uint64 const timestamp = (data [2] <<  0) |
					 (data [1] <<  8) |
					 (data [0] << 16);

		logD (time, _func, "rcv Type0 timestamp: 0x", fmt_hex, timestamp);

		bool has_extended_timestamp = false;
		if ((Uint32) timestamp == 0x00ffffff)
		    has_extended_timestamp = true;
                else
                if (momentrtmp_proto) {
                    recv_chunk_stream->in_msg_timestamp_low = timestamp;
		    has_extended_timestamp = true;
                } else
		    recv_chunk_stream->in_msg_timestamp = timestamp;

		recv_chunk_stream->in_msg_timestamp_delta = timestamp;
		recv_chunk_stream->in_msg_len = (data [5] <<  0) |
						(data [4] <<  8) |
						(data [3] << 16);
		recv_chunk_stream->in_msg_type_id = data [6];
		recv_chunk_stream->in_msg_stream_id = (data [ 7] <<  0) |
						      (data [ 8] <<  8) |
						      (data [ 9] << 16) |
						      (data [10] << 24);

		logD (msg, _func, "in_msg_len: ", recv_chunk_stream->in_msg_len, ", "
		      "in_msg_type_id: ", recv_chunk_stream->in_msg_type_id);

		logD (msg, _func, "in header is valid for chunk stream ", recv_chunk_stream->chunk_stream_id);
		recv_chunk_stream->in_header_valid = true;

		// TODO if (message is too long) ...

                if (dump_stream) {
                    dump_stream->println ("chunk header type 0 cs_id ", recv_chunk_stream->chunk_stream_id, " "
                                          "ts ", timestamp, " "
                                          "msg_len ", recv_chunk_stream->in_msg_len, " "
                                          "msg_type ", recv_chunk_stream->in_msg_type_id, " "
                                          "msg_stream_id ", recv_chunk_stream->in_msg_stream_id);
                }

		{
		    data += 11;
		    len -= 11;
		}

		if (has_extended_timestamp) {
		    extended_timestamp_is_delta = false;
		    conn_state = ReceiveState::ExtendedTimestamp;
		} else
		    conn_state = ReceiveState::ChunkData;
	    } break;
	    case ReceiveState::ChunkHeader_Type1: {
		logD (msg, _func, "ChunkHeader_Type1");

		if (len < 7) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}

		if (!recv_chunk_stream->in_header_valid) {
		    logE_ (_func, "in_header is not valid, type 1, cs id ", recv_chunk_stream->chunk_stream_id);
		    ret_res = Receiver::ProcessInputResult::Error;
		    goto _return;
		}

		Uint64 const timestamp_delta = (data [2] <<  0) |
					       (data [1] <<  8) |
					       (data [0] << 16);

		logD (time, _func, "rcv Type1 timestamp_delta: 0x", fmt_hex, timestamp_delta);

		bool has_extended_timestamp = false;
		if ((Uint32) timestamp_delta == 0x00ffffff)
		    has_extended_timestamp = true;
                else
                if (momentrtmp_proto) {
                    recv_chunk_stream->in_msg_timestamp_low = timestamp_delta;
                    has_extended_timestamp = true;
                } else
		    recv_chunk_stream->in_msg_timestamp += timestamp_delta;

		recv_chunk_stream->in_msg_timestamp_delta = timestamp_delta;
		recv_chunk_stream->in_msg_len = (data [5] <<  0) |
						(data [4] <<  8) |
						(data [3] << 16);
		recv_chunk_stream->in_msg_type_id = data [6];

		// TODO if (message is too long...)

                if (dump_stream) {
                    dump_stream->println ("chunk header type 1 cs_id ", recv_chunk_stream->chunk_stream_id, " "
                                          "ts_delta ", timestamp_delta, " "
                                          "msg_len ", recv_chunk_stream->in_msg_len, " "
                                          "msg_type ", recv_chunk_stream->in_msg_type_id);
                }

		{
		    data += 7;
		    len -= 7;
		}

		if (has_extended_timestamp) {
		    extended_timestamp_is_delta = true;
		    conn_state = ReceiveState::ExtendedTimestamp;
		} else
		    conn_state = ReceiveState::ChunkData;
	    } break;
	    case ReceiveState::ChunkHeader_Type2: {
		logD (msg, _func, "ChunkHeader_Type2");

		if (len < 3) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}

		if (!recv_chunk_stream->in_header_valid) {
		    logE_ (_func, "in_header is not valid, type 2, cs id ", recv_chunk_stream->chunk_stream_id);
		    ret_res = Receiver::ProcessInputResult::Error;
		    goto _return;
		}

		Uint64 const timestamp_delta = (data [2] <<  0) |
					       (data [1] <<  8) |
					       (data [0] << 16);

		logD (time, _func, "rcv Type2 timestamp_delta: 0x", fmt_hex, timestamp_delta);

		bool has_extended_timestamp = false;
		if ((Uint32) timestamp_delta == 0x00ffffff)
		    has_extended_timestamp = true;
                else
                if (momentrtmp_proto) {
                    recv_chunk_stream->in_msg_timestamp_low = timestamp_delta;
                    has_extended_timestamp = true;
                } else
		    recv_chunk_stream->in_msg_timestamp += timestamp_delta;

		recv_chunk_stream->in_msg_timestamp_delta = timestamp_delta;

                if (dump_stream) {
                    dump_stream->println ("chunk header type 1 cs_id ", recv_chunk_stream->chunk_stream_id, " "
                                          "ts_delta ", timestamp_delta);
                }

		{
		    data += 3;
		    len -= 3;
		}

		if (has_extended_timestamp) {
		    extended_timestamp_is_delta = true;
		    conn_state = ReceiveState::ExtendedTimestamp;
		} else
		    conn_state = ReceiveState::ChunkData;
	    } break;
	    case ReceiveState::ChunkHeader_Type3: {
		logD (msg, _func, "ChunkHeader_Type3");

		if (len < 1) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}

		if (!recv_chunk_stream->in_header_valid) {
		    logE_ (_func, "in_header is not valid, type 3, cs id ", recv_chunk_stream->chunk_stream_id);
		    ret_res = Receiver::ProcessInputResult::Error;
		    goto _return;
		}

		bool has_extended_timestamp = false;
		if ((Uint32) recv_chunk_stream->in_msg_timestamp_delta >= 0x00ffffff && !momentrtmp_proto)
		    has_extended_timestamp = true;

		if (recv_chunk_stream->in_msg_offset == 0)
		    recv_chunk_stream->in_msg_timestamp += recv_chunk_stream->in_msg_timestamp_delta;

		logD (msg, _func, "new msg timestamp: 0x", fmt_hex, recv_chunk_stream->in_msg_timestamp);

                if (dump_stream)
                    dump_stream->println ("chunk header type 3 cs_id ", recv_chunk_stream->chunk_stream_id);

		if (has_extended_timestamp) {
		    // XXX false or true?
		    // This doesn't matter as long as we simply ignore
		    // the extended timestamp field and use the old value.
		    extended_timestamp_is_delta = false;
		    ignore_extended_timestamp = true;
		    conn_state = ReceiveState::ExtendedTimestamp;
		} else {
		    conn_state = ReceiveState::ChunkData;
                }
	    } break;
	    case ReceiveState::ExtendedTimestamp: {
		logD (msg, _func, "ExtendedTimestamp");

		if (len < 4) {
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}

		if (recv_chunk_stream->in_msg_offset == 0 &&
		    !ignore_extended_timestamp)
		{
		    Uint64 extended_timestamp;
                    if (!momentrtmp_proto) {
                        extended_timestamp = ((Uint64) data [3] <<  0) |
                                             ((Uint64) data [2] <<  8) |
                                             ((Uint64) data [1] << 16) |
                                             ((Uint64) data [0] << 24);
                    } else {
                        extended_timestamp = recv_chunk_stream->in_msg_timestamp_low |
                                             ((Uint64) data [3] << 24) |
                                             ((Uint64) data [2] << 32) |
                                             ((Uint64) data [1] << 40) |
                                             ((Uint64) data [0] << 48);
                    }

		    if (extended_timestamp_is_delta)
			recv_chunk_stream->in_msg_timestamp += extended_timestamp;
		    else
			recv_chunk_stream->in_msg_timestamp = extended_timestamp;

                    if (dump_stream)
                        dump_stream->println ("extended timestamp ", extended_timestamp, " ts ", recv_chunk_stream->in_msg_timestamp);
		} else {
                    if (dump_stream) {
                        Uint32 const ts = ((Uint64) data [3] <<  0) |
                                          ((Uint64) data [2] <<  8) |
                                          ((Uint64) data [1] << 16) |
                                          ((Uint64) data [0] << 24);
                        dump_stream->println ("extended timestamp ", ts, " ignored");
                    }
                }
		ignore_extended_timestamp = false;

		{
		    data += 4;
		    len -= 4;
		}

		conn_state = ReceiveState::ChunkData;
	    } break;
	    case ReceiveState::ChunkData: {
		logD (msg, _func, "ChunkData");

		if (!((recv_chunk_stream->in_msg_offset < recv_chunk_stream->in_msg_len) ||
		      (recv_chunk_stream->in_msg_len == 0 && recv_chunk_stream->in_msg_offset == 0)))
		{
		    logE_ (_func, "bad chunking: in_msg_offset: ", recv_chunk_stream->in_msg_offset, ", "
			   "in_msg_len: ", recv_chunk_stream->in_msg_len);
		    ret_res = Receiver::ProcessInputResult::Error;
		    goto _return;
		}

		logD (msg, _func, "in_msg_len: ", recv_chunk_stream->in_msg_len, ", in_msg_offset: ", recv_chunk_stream->in_msg_offset);
		Size const msg_left = recv_chunk_stream->in_msg_len - recv_chunk_stream->in_msg_offset;
		if (msg_left <= in_chunk_size) {
		  // Last chunk of a message.

		    logD (msg, _func, "last chunk");

		    Size tofill = msg_left;
		    assert (chunk_offset <= tofill);
		    tofill -= chunk_offset;
		    if (tofill > len)
			tofill = len;

                    in_destr_mutex.lock ();
                    page_pool->getFillPages (&recv_chunk_stream->page_list,
                                             ConstMemory (data, tofill));
                    in_destr_mutex.unlock ();

		    {
			data += tofill;
			len -= tofill;
		    }

		    chunk_offset += tofill;
		    assert (chunk_offset <= msg_left);
		    if (chunk_offset < msg_left) {
			ret_res = Receiver::ProcessInputResult::Again;
			goto _return;
		    }

                    if (dump_stream) {
                        dump_stream->println ("full chunk csum ",
                                              calculateChecksumPages (recv_chunk_stream->page_list.first, 0 /* msg_offset */));
                    }

		    Result const res = processMessage (recv_chunk_stream);

		    resetMessageRecvState (recv_chunk_stream);
		    resetChunkRecvState ();

		    if (!res) {
			logD_ (_func, "processMessage() failed");
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }
		} else {
		  // Intermediate chunk.

		    logD (msg, _func, "intermediate chunk");

		    assert (chunk_offset < in_chunk_size);
		    Size tofill = in_chunk_size - chunk_offset;
		    if (tofill > len)
			tofill = len;

                    in_destr_mutex.lock ();
                    page_pool->getFillPages (&recv_chunk_stream->page_list,
                                             ConstMemory (data, tofill));
                    in_destr_mutex.unlock ();

		    {
			len -= tofill;
			data += tofill;
		    }

		    chunk_offset += tofill;
		    assert (chunk_offset <= in_chunk_size);
		    if (chunk_offset < in_chunk_size) {
			ret_res = Receiver::ProcessInputResult::Again;
			goto _return;
		    }

		    recv_chunk_stream->in_msg_offset += in_chunk_size;
		    resetChunkRecvState ();
		}
	    } break;
	    default:
		unreachable ();
	}
    } // for (;;)

_return:
    if (len != mem.len()) {
	if (len > mem.len()) {
	    logE_ (_func, "len > mem.len(): ", len, " > ", mem.len());
	    unreachable ();
	}
    }

    assert (len <= mem.len());
    *ret_accepted = mem.len() - len;

    processing_input = false;

    return ret_res;
}

mt_sync_domain (receiver) void
RtmpConnection::processEof (Memory   const /* unprocessed_mem */,
                            void   * const _self)
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) _self);

    RtmpConnection * const self = static_cast <RtmpConnection*> (_self);

    self->frontend.call (self->frontend->closed, /*(*/ (Exception*) NULL /* exc */);
    self->backend.call (self->backend->close, DisconnectReason::UnderlyingConnectionTerminated);
}

mt_sync_domain (receiver) void
RtmpConnection::processError (Exception * const exc_,
                              Memory      const /* unprocessed_mem */,
			      void      * const _self)
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) _self);

    RtmpConnection * const self = static_cast <RtmpConnection*> (_self);
    self->doError (exc_);
}

void
RtmpConnection::doError (Exception * const exc_)
{
    frontend.call (frontend->closed, /*(*/ exc_ /*)*/);
    backend.call (backend->close, DisconnectReason::TodoCategorizeMe);
}

void
RtmpConnection::reportError ()
{
    InternalException internal_exc (InternalException::FrontendError);
    doError (&internal_exc);
}

mt_const void
RtmpConnection::startClient ()
{
    conn_state = ReceiveState::ClientWaitS0;

    PagePool::PageListInfo page_list;
    page_pool->getPages (&page_list, 1537 /* len */);
    assert (page_list.first->data_len >= 1537);
    Byte * const msg_c1 = page_list.first->getData();
    msg_c1 [0] = momentrtmp_proto ? 0xff : 3;
    {
	Uint32 const time = getTimeMicroseconds ();
	msg_c1 [1] = (time >>  0) & 0xff;
	msg_c1 [2] = (time >>  8) & 0xff;
	msg_c1 [3] = (time >> 16) & 0xff;
	msg_c1 [4] = (time >> 24) & 0xff;
    }

    memset (msg_c1 + 5, 0, 4);
    {
	unsigned n = 0;
	for (unsigned i = 9; i < 1537; ++i) {
	    n = (1536 + i + n) % 317;
	    msg_c1 [i] = n;
	}
    }

    sendRawPages (page_list.first, page_list.data_len);

    beginPings ();
}

mt_const void
RtmpConnection::startServer ()
{
    conn_state = ReceiveState::ServerWaitC0;
    beginPings ();

    MOMENT__RTMP_INIT
}

Result
RtmpConnection::doCreateStream (Uint32       const msg_stream_id,
				AmfDecoder * const mt_nonnull amf_decoder)
{
    double transaction_id;
    if (!amf_decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    // TODO Perhaps a unique message stream id should be allocated.
    //      (a simple increment of a counter would do).
    double const reply_msg_stream_id = DefaultMessageStreamId;

    {
	AmfAtom atoms [4];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();
	encoder.addNumber (reply_msg_stream_id);

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "encode() failed");
	    return Result::Failure;
	}

	sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    return Result::Success;
}

Result
RtmpConnection::doReleaseStream (Uint32       const msg_stream_id,
				 AmfDecoder * const mt_nonnull amf_decoder)
{
    double transaction_id;
    if (!amf_decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    {
	AmfAtom atoms [3];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "encode() failed");
	    return Result::Failure;
	}

	sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    return Result::Success;
}

Result
RtmpConnection::doCloseStream (Uint32       const msg_stream_id,
			       AmfDecoder * const mt_nonnull amf_decoder)
{
    double transaction_id;
    if (!amf_decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    {
	AmfAtom atoms [3];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "encode() failed");
	    return Result::Failure;
	}

	sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    return Result::Success;
}

Result
RtmpConnection::doDeleteStream (Uint32       const msg_stream_id,
				AmfDecoder * const mt_nonnull amf_decoder)
{
    double transaction_id;
    if (!amf_decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    {
	AmfAtom atoms [3];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "encode() failed");
	    return Result::Failure;
	}

	sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    return Result::Success;
}

Result
RtmpConnection::doBasicMessage (Uint32       const msg_stream_id,
				AmfDecoder * const mt_nonnull amf_decoder)
{
    double transaction_id;
    if (!amf_decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    {
	AmfAtom atoms [3];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "encode() failed");
	    return Result::Failure;
	}

	sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    return Result::Success;
}

Result
RtmpConnection::fireVideoMessage (VideoMessage * const mt_nonnull video_msg)
{
    Result res = Result::Failure;
    frontend.call_ret<Result> (&res, frontend->videoMessage, /*(*/ video_msg /*)*/);
    return res;
}

mt_const void
RtmpConnection::init (OutputStream * const dump_stream,
                      Timers       * const mt_nonnull timers,
		      PagePool     * const mt_nonnull page_pool,
		      Time           const send_delay_millisec,
                      Time           const ping_timeout_millisec,
                      bool           const momentrtmp_proto)
{
    this->dump_stream           = dump_stream;
    this->timers                = timers;
    this->page_pool             = page_pool;
    this->send_delay_millisec   = send_delay_millisec;
    this->ping_timeout_millisec = ping_timeout_millisec;
    this->momentrtmp_proto      = momentrtmp_proto;
}

RtmpConnection::RtmpConnection (EmbedContainer * const embed_container)
    : Object (embed_container),

      dump_stream (NULL),

      send_delay_millisec (0),
      ping_timeout_millisec (5 * 60 * 1000),

      // First timeout period has double duration.
      ping_reply_received (1),

      in_chunk_size  (DefaultChunkSize),
      out_chunk_size (DefaultChunkSize),

      out_got_first_timestamp (false),
      out_first_timestamp (0),
      out_first_frames_counter (0),

      out_last_flush_time (0),

      extended_timestamp_is_delta (false),
      ignore_extended_timestamp (false),

      processing_input (false),

      remote_wack_size (1 << 20 /* 1 Mb */),

      total_received (0),
      last_ack (0),

      conn_state (ReceiveState::Invalid),

      local_wack_size (1 << 20 /* 1 Mb */)
{
    resetChunkRecvState ();

    control_chunk_stream = getChunkStream (2, true /* create */);
    data_chunk_stream    = getChunkStream (3, true /* create */);
    audio_chunk_stream   = getChunkStream (4, true /* create */);
    video_chunk_stream   = getChunkStream (5, true /* create */);
}

RtmpConnection::~RtmpConnection ()
{
    // closed() event should always be fired for frontend listeners
    // before RtmpConnection is destroyed. This extra call ensures that.
    if (frontend)
        frontend.call (frontend->closed, /*(*/ (Exception*) NULL /*)*/);

    in_destr_mutex.lock ();

    if (ping_send_timer) {
	timers->deleteTimer (ping_send_timer);
        ping_send_timer = NULL;
    }

    {
	ChunkStreamTree::Iterator iter (chunk_stream_tree);
	while (!iter.done ()) {
	    StRef<ChunkStream> const &chunk_stream = iter.next ().value;
	    releaseChunkStream (chunk_stream);
	}
    }

    in_destr_mutex.unlock ();
}

}

