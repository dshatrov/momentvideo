/*  Copyright (C) 2011-2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__LIBMARY__H__
#define LIBMARY__LIBMARY__H__


#include <libmary/annotations.h>
#include <libmary/types_base.h>
#include <libmary/types.h>
#include <libmary/string.h>
#include <libmary/exception.h>
#include <libmary/informer.h>

#include <libmary/extractor.h>
#include <libmary/comparator.h>
#include <libmary/iterator.h>

#include <libmary/array.h>
#include <libmary/array_holder.h>
#include <libmary/list.h>
#include <libmary/intrusive_list.h>
#include <libmary/avl_tree.h>
#include <libmary/intrusive_avl_tree.h>
#include <libmary/map.h>
#include <libmary/hash.h>
#include <libmary/string_hash.h>
#include <libmary/namespace_container.h>
#include <libmary/page_pool.h>
#include <libmary/vstack.h>
#include <libmary/vslab.h>
#include <libmary/preassembly_buffer.h>

#include <libmary/atomic.h>
#include <libmary/mutex.h>
#include <libmary/fast_mutex.h>
#include <libmary/state_mutex.h>
#ifdef LIBMARY_MT_SAFE
  #include <libmary/cond.h>
  #include <libmary/fast_cond.h>
  #include <libmary/thread.h>
  #include <libmary/multi_thread.h>
  #include <libmary/blocking_queue.h>
#endif

#include <libmary/st_referenced.h>
#include <libmary/virt_referenced.h>
#include <libmary/referenced.h>
#include <libmary/object.h>

#include <libmary/st_ref.h>
#include <libmary/ref.h>
#include <libmary/weak_ref.h>

#include <libmary/process.h>
#include <libmary/timers.h>

#include <libmary/io.h>
#include <libmary/log.h>
#include <libmary/file.h>
#include <libmary/async_file.h>
#include <libmary/memory_file.h>
#include <libmary/cached_file.h>
#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/native_file.linux.h>
  #include <libmary/native_async_file.linux.h>
  #include <libmary/udp_socket.linux.h>
  #include <libmary/tcp_connection.linux.h>
  #include <libmary/tcp_server.linux.h>
  #include <libmary/connection_receiver.generic.h>
#else
  #include <libmary/native_file.h>
  #include <libmary/native_async_file.h>
  #include <libmary/udp_socket.h>
  #include <libmary/tcp_connection.h>
  #include <libmary/tcp_server.h>
  #include <libmary/connection_receiver.h>
#endif
#include <libmary/output_stream.h>
#include <libmary/buffered_output_stream.h>
// Unused #include <libmary/array_output_stream.h>
#include <libmary/log_rotate_stream.h>

#include <libmary/async_input_stream.h>
#include <libmary/async_output_stream.h>
#include <libmary/connection.h>
#include <libmary/file_connection.h>
#include <libmary/sender_message_entry.h>
#include <libmary/sender.h>
#include <libmary/immediate_connection_sender.h>
#include <libmary/deferred_connection_sender.h>
#include <libmary/sync_stream_sender.h>
#include <libmary/receiver.h>

#include <libmary/message_server.h>
#include <libmary/message_service.h>
#include <libmary/line_server.h>
#include <libmary/line_service.h>
#ifndef LIBMARY_PLATFORM_WIN32
  #include <libmary/line_pipe.h>
  #include <libmary/line_fd_pipe.h>
#endif

#include <libmary/vfs.h>

#include <libmary/deferred_processor.h>
#include <libmary/poll_group.h>
#include <libmary/active_poll_group.h>

#include <libmary/http_parser.h>
#include <libmary/http_server.h>
#include <libmary/http_client.h>
#include <libmary/http_service.h>

#include <libmary/module.h>

#include <libmary/util_common.h>
#include <libmary/util_base.h>
#include <libmary/util_mem.h>
#include <libmary/util_str.h>
#include <libmary/util_time.h>
#include <libmary/util_net.h>
#include <libmary/util_dev.h>
#include <libmary/base64.h>
#include <libmary/libmary_md5.h>
#include <libmary/cmdline.h>
#include <libmary/address_saver.h>
#include <libmary/linear_reader.h>

#include <libmary/server_context.h>
#include <libmary/server_thread_pool.h>
#include <libmary/fixed_thread_pool.h>
#include <libmary/server_app.h>

#include <libmary/stat.h>
#include <libmary/stat_counter.h>


namespace M {

void libMaryInit ();

void libMaryRelease ();

}


#endif /* LIBMARY__LIBMARY__H__ */

