#TARGET_PLATFORM = linux
TARGET_PLATFORM = macosx
#TARGET_PLATFORM = android

ifeq ($(TARGET_PLATFORM), macosx)
    CC  = clang
    CXX = clang++
endif
ifeq ($(TARGET_PLATFORM), linux)
    CC  = LD_LIBRARY_PATH=/opt/gcc/lib /opt/gcc/bin/gcc
    CXX = LD_LIBRARY_PATH=/opt/gcc/lib /opt/gcc/bin/g++
endif
ifeq ($(TARGET_PLATFORM), android)
    MOMENT__ANDROID_NDK_PLATFORM="android-9"
    MOMENT__ANDROID_TOOLCHAIN_CROSS_PREFIX="arm-linux-androideabi"
    MOMENT__ANDROID_TOOLCHAIN_NAME="$(MOMENT__ANDROID_TOOLCHAIN_CROSS_PREFIX)-4.8"
    MOMENT__ANDROID_TOOLCHAIN_HOME="$(HOME)/files/my_dev_soft/android_toolchains/$(MOMENT__ANDROID_TOOLCHAIN_NAME)"

    MOMENT__ANDROID_CROSS="$(MOMENT__ANDROID_TOOLCHAIN_HOME)/bin/$(MOMENT__ANDROID_TOOLCHAIN_CROSS_PREFIX)"

    CC  = "$(MOMENT__ANDROID_CROSS)-gcc"
    CXX = "$(MOMENT__ANDROID_CROSS)-g++"
    CPP = "$(MOMENT__ANDROID_CROSS)-cpp"
    LD  = "$(MOMENT__ANDROID_CROSS)-ld"
    AR  = "$(MOMENT__ANDROID_CROSS)-ar"
    AS  = "$(MOMENT__ANDROID_CROSS)-as"
    NM  = "$(MOMENT__ANDROID_CROSS)-nm"
    RANLIB = "$(MOMENT__ANDROID_CROSS)-ranlib"
endif

#COMMON_CFLAGS = -Os -flto -I "include/include_config/$(TARGET_PLATFORM)" -DMOMENT_STATIC_MODULES -DLIBMARY_NO_EXCEPTIONS
COMMON_CFLAGS = -O0 -g    -I "include/include_config/$(TARGET_PLATFORM)" -DMOMENT_STATIC_MODULES -DLIBMARY_NO_EXCEPTIONS

ifeq ($(TARGET_PLATFORM), android)
    COMMON_CFLAGS += -fPIC
endif

ifneq ($(TARGET_PLATFORM), macosx)
    CFLAGS   = $(COMMON_CFLAGS) -std=gnu99 -pthread
    CXXFLAGS = $(COMMON_CFLAGS) -std=c++11 -pthread -fno-exceptions -fno-rtti
else
    CFLAGS   = $(COMMON_CFLAGS) -std=gnu99
    CXXFLAGS = $(COMMON_CFLAGS) -std=c++11 -fno-exceptions -fno-rtti
endif

CC_COMMAND  = $(CC)  $(CFLAGS)   -MD -c -o $@ $<
CXX_COMMAND = $(CXX) $(CXXFLAGS) -MD -c -o $@ $<
AR_COMMAND  = $(AR) -rs $@ $^

ifeq ($(TARGET_PLATFORM), macosx)
    CXX_LINK_COMMAND = $(CXX) $(CXXFLAGS) -o $@ $^ -dead_strip -Wl,-undefined,error
endif
ifeq ($(TARGET_PLATFORM), linux)
    CXX_LINK_COMMAND = $(CXX) $(CXXFLAGS) -o $@ $^ -static-libgcc -static-libstdc++ -lrt
endif
ifeq ($(TARGET_PLATFORM), android)
    CXX_LINK_COMMAND = $(CXX) $(CXXFLAGS) -o $@ $^ -llog
endif

TARGET_BUILD_DIR = build/$(TARGET_PLATFORM)

LIBMARY_BUILD_DIR        = $(TARGET_BUILD_DIR)/libmary_build
PARGEN_BUILD_DIR         = $(TARGET_BUILD_DIR)/pargen_build
SCRUFFY_BUILD_DIR        = $(TARGET_BUILD_DIR)/scruffy_build
MCONFIG_BUILD_DIR        = $(TARGET_BUILD_DIR)/mconfig_build
MOMENT_BUILD_DIR         = $(TARGET_BUILD_DIR)/moment_build
LIBMOMENT_RTMP_BUILD_DIR = $(TARGET_BUILD_DIR)/moment-rtmp_build
LIBMOMENT_RTSP_BUILD_DIR = $(TARGET_BUILD_DIR)/moment-rtsp_build
MOMENT_HLS_BUILD_DIR     = $(TARGET_BUILD_DIR)/moment-hls_build
MOMENT_VOD_BUILD_DIR     = $(TARGET_BUILD_DIR)/moment-vod_build
MOMENT_AUTH_BUILD_DIR    = $(TARGET_BUILD_DIR)/moment-auth_build
MOMENT_NVR_BUILD_DIR     = $(TARGET_BUILD_DIR)/moment-nvr_build
MOMENT_THUMB_BUILD_DIR   = $(TARGET_BUILD_DIR)/moment-thumb_build
MOMENT_TEST_BUILD_DIR    = $(TARGET_BUILD_DIR)/moment-test_build

LIBMARY_TARGET            = $(LIBMARY_BUILD_DIR)/libmary.a
TEST_LIBMARY_TARGET       = $(LIBMARY_BUILD_DIR)/test_libmary
PARGEN_TARGET             = $(PARGEN_BUILD_DIR)/libpargen.a
SCRUFFY_TARGET            = $(SCRUFFY_BUILD_DIR)/libscruffy.a
MCONFIG_TARGET            = $(MCONFIG_BUILD_DIR)/libmconfig.a
LIBMOMENT_TARGET          = $(MOMENT_BUILD_DIR)/libmoment.a
MOMENT_MOD_TEST_TARGET    = $(MOMENT_BUILD_DIR)/moment-mod-test.a
MOMENT_FILE_TARGET        = $(MOMENT_BUILD_DIR)/moment-file.a
LIBMOMENT_INSTANCE_TARGET = $(MOMENT_BUILD_DIR)/libmoment-instance.a
MOMENT_TARGET             = $(MOMENT_BUILD_DIR)/moment
HLSTOOL_TARGET            = $(MOMENT_BUILD_DIR)/hlstool
LIBMOMENT_RTMP_TARGET     = $(LIBMOMENT_RTMP_BUILD_DIR)/libmoment-rtmp.a
MOMENT_RTMP_TARGET        = $(LIBMOMENT_RTMP_BUILD_DIR)/moment-rtmp.a
RTMPTOOL_TARGET           = $(LIBMOMENT_RTMP_BUILD_DIR)/rtmptool
LIBMOMENT_RTSP_TARGET     = $(LIBMOMENT_RTSP_BUILD_DIR)/libmoment-rtsp.a
MOMENT_RTSP_TARGET        = $(LIBMOMENT_RTSP_BUILD_DIR)/moment-rtsp.a
RTSPTOOL_TARGET           = $(LIBMOMENT_RTSP_BUILD_DIR)/rtsptool
MOMENT_HLS_TARGET         = #$(MOMENT_HLS_BUILD_DIR)/moment-hls.a
MOMENT_VOD_TARGET         = $(MOMENT_VOD_BUILD_DIR)/moment-vod.a
MOMENT_AUTH_TARGET        = $(MOMENT_AUTH_BUILD_DIR)/moment-auth.a
MOMENT_NVR_TARGET         = $(MOMENT_NVR_BUILD_DIR)/moment-nvr.a
MOMENT_THUMB_TARGET       = $(MOMENT_THUMB_BUILD_DIR)/moment-thumb.a
MOMENT_TEST_TARGET        = $(MOMENT_TEST_BUILD_DIR)/moment-test

TESTS = $(LIBMARY_BUILD_DIR)/test_libmary.stamp

ifeq ($(TARGET_PLATFORM), android)
all: all_notests
else
all: all_notests $(TESTS)
endif

ifeq ($(TARGET_PLATFORM), macosx)
all_notests:                         \
        $(LIBMARY_TARGET)            \
        $(PARGEN_TARGET)             \
        $(SCRUFFY_TARGET)            \
        $(MCONFIG_TARGET)            \
        $(LIBMOMENT_TARGET)          \
        $(MOMENT_MOD_TEST_TARGET)    \
        $(MOMENT_FILE_TARGET)        \
        $(LIBMOMENT_INSTANCE_TARGET) \
        $(MOMENT_TARGET)             \
        $(HLSTOOL_TARGET)            \
        $(LIBMOMENT_RTMP_TARGET)     \
        $(MOMENT_RTMP_TARGET)        \
        $(RTMPTOOL_TARGET)           \
        $(LIBMOMENT_RTSP_TARGET)     \
        $(MOMENT_RTSP_TARGET)        \
        $(RTSPTOOL_TARGET)           \
        $(MOMENT_HLS_TARGET)         \
        $(MOMENT_VOD_TARGET)         \
        $(MOMENT_AUTH_TARGET)        \
        $(MOMENT_NVR_TARGET)         \
        $(MOMENT_TEST_TARGET)
#        $(MOMENT_THUMB_TARGET)
else
all_notests:                         \
        $(MOMENT_TARGET)             \
        $(LIBMOMENT_INSTANCE_TARGET) \
        $(HLSTOOL_TARGET)            \
        $(RTMPTOOL_TARGET)           \
        $(RTSPTOOL_TARGET)           \
        $(MOMENT_HLS_TARGET)         \
        $(MOMENT_VOD_TARGET)         \
        $(MOMENT_AUTH_TARGET)        \
        $(MOMENT_NVR_TARGET)         \
        $(MOMENT_TEST_TARGET)        \
        $(MOMENT_MOD_TEST_TARGET)    \
        $(MOMENT_FILE_TARGET)        \
        $(MOMENT_RTMP_TARGET)        \
        $(LIBMOMENT_RTMP_TARGET)     \
        $(MOMENT_RTSP_TARGET)        \
        $(LIBMOMENT_RTSP_TARGET)     \
        $(LIBMOMENT_TARGET)          \
        $(MCONFIG_TARGET)            \
        $(SCRUFFY_TARGET)            \
        $(PARGEN_TARGET)             \
        $(LIBMARY_TARGET)
endif

.PHONY: all test clean

LIBMARY_OBJECTS =                                           \
        $(LIBMARY_BUILD_DIR)/util_c.o                       \
        $(LIBMARY_BUILD_DIR)/util_base.o                    \
        $(LIBMARY_BUILD_DIR)/util_common.o                  \
        $(LIBMARY_BUILD_DIR)/util_str.o                     \
        $(LIBMARY_BUILD_DIR)/util_time.o                    \
        $(LIBMARY_BUILD_DIR)/util_net.o                     \
        $(LIBMARY_BUILD_DIR)/util_dev.o                     \
        $(LIBMARY_BUILD_DIR)/util_posix.o                   \
        $(LIBMARY_BUILD_DIR)/base64.o                       \
        $(LIBMARY_BUILD_DIR)/cmdline.o                      \
                                                            \
        $(LIBMARY_BUILD_DIR)/types.o                        \
        $(LIBMARY_BUILD_DIR)/string.o                       \
        $(LIBMARY_BUILD_DIR)/exception.o                    \
        $(LIBMARY_BUILD_DIR)/informer.o                     \
                                                            \
        $(LIBMARY_BUILD_DIR)/page_pool.o                    \
        $(LIBMARY_BUILD_DIR)/streaming_page_pool.o          \
        $(LIBMARY_BUILD_DIR)/vstack.o                       \
        $(LIBMARY_BUILD_DIR)/preassembly_buffer.o           \
                                                            \
        $(LIBMARY_BUILD_DIR)/state_mutex.o                  \
                                                            \
        $(LIBMARY_BUILD_DIR)/referenced.o                   \
        $(LIBMARY_BUILD_DIR)/object.o                       \
        $(LIBMARY_BUILD_DIR)/deletion_context.o             \
                                                            \
        $(LIBMARY_BUILD_DIR)/libmary_thread_local.o         \
        $(LIBMARY_BUILD_DIR)/deletion_queue.o               \
                                                            \
        $(LIBMARY_BUILD_DIR)/timers.o                       \
                                                            \
        $(LIBMARY_BUILD_DIR)/log.o                          \
        $(LIBMARY_BUILD_DIR)/input_stream.o                 \
        $(LIBMARY_BUILD_DIR)/output_stream.o                \
        $(LIBMARY_BUILD_DIR)/buffered_output_stream.o       \
        $(LIBMARY_BUILD_DIR)/file.o                         \
        $(LIBMARY_BUILD_DIR)/memory_file.o                  \
        $(LIBMARY_BUILD_DIR)/cached_file.o                  \
        $(LIBMARY_BUILD_DIR)/log_rotate_stream.o            \
        $(LIBMARY_BUILD_DIR)/android_logcat_output_stream.o \
                                                            \
        $(LIBMARY_BUILD_DIR)/vfs.o                          \
        $(LIBMARY_BUILD_DIR)/vfs_posix.o                    \
                                                            \
        $(LIBMARY_BUILD_DIR)/async_output_stream.o          \
        $(LIBMARY_BUILD_DIR)/file_connection.o              \
        $(LIBMARY_BUILD_DIR)/sender_message_entry.o         \
        $(LIBMARY_BUILD_DIR)/sender.o                       \
        $(LIBMARY_BUILD_DIR)/connection_sender_impl.o       \
        $(LIBMARY_BUILD_DIR)/immediate_connection_sender.o  \
        $(LIBMARY_BUILD_DIR)/deferred_connection_sender.o   \
        $(LIBMARY_BUILD_DIR)/sync_stream_sender.o           \
        $(LIBMARY_BUILD_DIR)/receiver.o                     \
                                                            \
        $(LIBMARY_BUILD_DIR)/message_server.o               \
        $(LIBMARY_BUILD_DIR)/message_service.o              \
        $(LIBMARY_BUILD_DIR)/line_server.o                  \
        $(LIBMARY_BUILD_DIR)/line_service.o                 \
                                                            \
        $(LIBMARY_BUILD_DIR)/deferred_processor.o           \
                                                            \
        $(LIBMARY_BUILD_DIR)/http_request.o                 \
        $(LIBMARY_BUILD_DIR)/http_server.o                  \
        $(LIBMARY_BUILD_DIR)/http_client.o                  \
        $(LIBMARY_BUILD_DIR)/http_service.o                 \
                                                            \
        $(LIBMARY_BUILD_DIR)/module.o                       \
                                                            \
        $(LIBMARY_BUILD_DIR)/fixed_thread_pool.o            \
        $(LIBMARY_BUILD_DIR)/server_app.o                   \
                                                            \
        $(LIBMARY_BUILD_DIR)/stat.o                         \
        $(LIBMARY_BUILD_DIR)/stat_counter.o                 \
                                                            \
        $(LIBMARY_BUILD_DIR)/md5/md5.o                      \
        $(LIBMARY_BUILD_DIR)/libmary_md5.o                  \
                                                            \
        $(LIBMARY_BUILD_DIR)/libmary.o                      \
                                                            \
        $(LIBMARY_BUILD_DIR)/thread.o                       \
        $(LIBMARY_BUILD_DIR)/multi_thread.o                 \
                                                            \
        $(LIBMARY_BUILD_DIR)/posix.o                        \
        $(LIBMARY_BUILD_DIR)/common_socket_posix.o          \
        $(LIBMARY_BUILD_DIR)/process.unix.o                 \
        $(LIBMARY_BUILD_DIR)/native_file.linux.o            \
        $(LIBMARY_BUILD_DIR)/native_async_file.linux.o      \
        $(LIBMARY_BUILD_DIR)/udp_socket.linux.o             \
        $(LIBMARY_BUILD_DIR)/tcp_connection.linux.o         \
        $(LIBMARY_BUILD_DIR)/tcp_server.linux.o             \
        $(LIBMARY_BUILD_DIR)/select_poll_group.o            \
        $(LIBMARY_BUILD_DIR)/poll_poll_group.o              \
        $(LIBMARY_BUILD_DIR)/line_pipe.o                    \
        $(LIBMARY_BUILD_DIR)/line_fd_pipe.o                 \
        $(LIBMARY_BUILD_DIR)/connection_receiver.generic.o  \
                                                            \
        $(LIBMARY_BUILD_DIR)/epoll_poll_group.o

TEST_LIBMARY_OBJECTS =                                         \
        $(LIBMARY_BUILD_DIR)/test_libmary.o                    \
        $(LIBMARY_BUILD_DIR)/test_libmary__base64.o            \
        $(LIBMARY_BUILD_DIR)/test_libmary__log_rotate_stream.o \
        $(LIBMARY_BUILD_DIR)/test_libmary__object.o            \
        $(LIBMARY_BUILD_DIR)/test_libmary__ref.o               \
        $(LIBMARY_BUILD_DIR)/test_libmary__referenced.o        \
        $(LIBMARY_BUILD_DIR)/test_libmary__sender.o

PARGEN_OBJECTS =                                  \
        $(PARGEN_BUILD_DIR)/file_token_stream.o   \
        $(PARGEN_BUILD_DIR)/memory_token_stream.o \
        $(PARGEN_BUILD_DIR)/grammar.o             \
        $(PARGEN_BUILD_DIR)/parser.o

SCRUFFY_OBJECTS =                                          \
        $(SCRUFFY_BUILD_DIR)/pp_item_stream.o              \
        $(SCRUFFY_BUILD_DIR)/unichar_pp_item_stream.o      \
        $(SCRUFFY_BUILD_DIR)/list_pp_item_stream.o         \
        $(SCRUFFY_BUILD_DIR)/phase3_item_stream.o          \
        $(SCRUFFY_BUILD_DIR)/pp_item_stream_token_stream.o \
        $(SCRUFFY_BUILD_DIR)/util.o                        \
        $(SCRUFFY_BUILD_DIR)/checkpoint_tracker.o          \
        $(SCRUFFY_BUILD_DIR)/cpp_cond_pargen.o             \
        $(SCRUFFY_BUILD_DIR)/file_byte_stream.o            \
        $(SCRUFFY_BUILD_DIR)/utf8_unichar_stream.o         \
        $(SCRUFFY_BUILD_DIR)/preprocessor_util.o           \
        $(SCRUFFY_BUILD_DIR)/preprocessor.o

SCRUFFY_GENFILES =                          \
        scruffy/scruffy/cpp_cond_pargen.h   \
        scruffy/scruffy/cpp_cond_pargen.cpp

scruffy/scruffy/cpp_cond_pargen.cpp: scruffy/scruffy/cpp_cond.par
	sh -c "cd scruffy/scruffy && pargen --module-name scruffy --header-name cpp_cond cpp_cond.par"
scruffy/scruffy/cpp_cond_pargen.h: scruffy/scruffy/cpp_cond_pargen.cpp

MCONFIG_OBJECTS =                             \
        $(MCONFIG_BUILD_DIR)/mconfig.o        \
        $(MCONFIG_BUILD_DIR)/util.o           \
        $(MCONFIG_BUILD_DIR)/config.o         \
        $(MCONFIG_BUILD_DIR)/varlist.o        \
        $(MCONFIG_BUILD_DIR)/config_parser.o  \
        $(MCONFIG_BUILD_DIR)/varlist_parser.o \
        $(MCONFIG_BUILD_DIR)/mconfig_pargen.o \
        $(MCONFIG_BUILD_DIR)/varlist_pargen.o

MCONFIG_GENFILES =                         \
        mconfig/mconfig/mconfig_pargen.h   \
        mconfig/mconfig/mconfig_pargen.cpp \
        mconfig/mconfig/varlist_pargen.h   \
        mconfig/mconfig/varlist_pargen.cpp

mconfig/mconfig/mconfig_pargen.cpp: mconfig/mconfig/mconfig.par
	sh -c "cd mconfig/mconfig && pargen --extmode --module-name MConfig --header-name mconfig mconfig.par"
mconfig/mconfig/mconfig_pargen.h: mconfig/mconfig/mconfig_pargen.cpp

mconfig/mconfig/varlist_pargen.cpp: mconfig/mconfig/varlist.par
	sh -c "cd mconfig/mconfig && pargen --extmode --module-name VarList --namespace MConfig --header-name varlist varlist.par"
mconfig/mconfig/varlist_pargen.h: mconfig/mconfig/varlist_pargen.cpp

LIBMOMENT_OBJECTS =                                 \
        $(MOMENT_BUILD_DIR)/media_message.o         \
        $(MOMENT_BUILD_DIR)/media_desc.o            \
        $(MOMENT_BUILD_DIR)/frame_saver.o           \
        $(MOMENT_BUILD_DIR)/media_source.o          \
        $(MOMENT_BUILD_DIR)/stream.o                \
        $(MOMENT_BUILD_DIR)/av_stream_group.o       \
                                                    \
        $(MOMENT_BUILD_DIR)/libmoment.o             \
                                                    \
        $(MOMENT_BUILD_DIR)/channel.o               \
        $(MOMENT_BUILD_DIR)/playback_item.o         \
        $(MOMENT_BUILD_DIR)/channel_options.o       \
        $(MOMENT_BUILD_DIR)/channel_set.o           \
        $(MOMENT_BUILD_DIR)/channel_manager.o       \
        $(MOMENT_BUILD_DIR)/slave_stream_source.o   \
        $(MOMENT_BUILD_DIR)/playback.o              \
        $(MOMENT_BUILD_DIR)/playlist.o              \
        $(MOMENT_BUILD_DIR)/recorder.o              \
                                                    \
        $(MOMENT_BUILD_DIR)/h264_parser.o           \
        $(MOMENT_BUILD_DIR)/flv_util.o              \
        $(MOMENT_BUILD_DIR)/amf_encoder.o           \
        $(MOMENT_BUILD_DIR)/amf_decoder.o           \
                                                    \
        $(MOMENT_BUILD_DIR)/test_stream_generator.o \
                                                    \
        $(MOMENT_BUILD_DIR)/rate_limit.o            \
        $(MOMENT_BUILD_DIR)/vod_reader.o            \
        $(MOMENT_BUILD_DIR)/mp4_reader.o            \
        $(MOMENT_BUILD_DIR)/vod_source.o            \
        $(MOMENT_BUILD_DIR)/reader_vod_source.o     \
                                                    \
        $(MOMENT_BUILD_DIR)/av_recorder.o           \
        $(MOMENT_BUILD_DIR)/flv_muxer.o             \
        $(MOMENT_BUILD_DIR)/mp4_av_muxer.o          \
        $(MOMENT_BUILD_DIR)/mp4_muxer.o             \
        $(MOMENT_BUILD_DIR)/ts_muxer.o              \
        $(MOMENT_BUILD_DIR)/ts_demuxer.o            \
                                                    \
        $(MOMENT_BUILD_DIR)/local_storage.o         \
                                                    \
        $(MOMENT_BUILD_DIR)/push_agent.o            \
                                                    \
        $(MOMENT_BUILD_DIR)/fetch_agent.o           \
                                                    \
        $(MOMENT_BUILD_DIR)/event_service.o         \
        $(MOMENT_BUILD_DIR)/client_manager.o        \
        $(MOMENT_BUILD_DIR)/stream_manager.o        \
        $(MOMENT_BUILD_DIR)/media_manager.o         \
        $(MOMENT_BUILD_DIR)/source_manager.o        \
        $(MOMENT_BUILD_DIR)/http_manager.o          \
        $(MOMENT_BUILD_DIR)/config_manager.o        \
        $(MOMENT_BUILD_DIR)/page_request_manager.o  \
        $(MOMENT_BUILD_DIR)/transcoding_manager.o   \
        $(MOMENT_BUILD_DIR)/auth_manager.o          \
        $(MOMENT_BUILD_DIR)/moment_server.o         \
                                                    \
        $(MOMENT_BUILD_DIR)/util_moment.o           \
        $(MOMENT_BUILD_DIR)/util_config.o           \
        $(MOMENT_BUILD_DIR)/util_h264.o             \
                                                    \
        $(MOMENT_BUILD_DIR)/realtime_hls_client.o   \
        $(MOMENT_BUILD_DIR)/realtime_hls_fetch_protocol.o

# Unused
#        $(MOMENT_BUILD_DIR)/mtp_connection.o

MOMENT_MOD_TEST_OBJECTS = \
        $(MOMENT_BUILD_DIR)/mod_test.o

MOMENT_FILE_OBJECTS = \
        $(MOMENT_BUILD_DIR)/mod_file.o

LIBMOMENT_INSTANCE_OBJECTS =                  \
        $(MOMENT_BUILD_DIR)/moment_instance.o \
        $(MOMENT_BUILD_DIR)/moment_modules.o

MOMENT_OBJECTS = \
        $(MOMENT_BUILD_DIR)/moment.o

HLSTOOL_OBJECTS = \
        $(MOMENT_BUILD_DIR)/hlstool.o

LIBMOMENT_RTMP_OBJECTS =                                  \
        $(LIBMOMENT_RTMP_BUILD_DIR)/rtmp_connection.o     \
        $(LIBMOMENT_RTMP_BUILD_DIR)/rtmp_client.o         \
        $(LIBMOMENT_RTMP_BUILD_DIR)/rtmp_server.o         \
        $(LIBMOMENT_RTMP_BUILD_DIR)/rtmp_service.o        \
        $(LIBMOMENT_RTMP_BUILD_DIR)/rtmpt_service.o       \
        $(LIBMOMENT_RTMP_BUILD_DIR)/rtmp_push_protocol.o  \
        $(LIBMOMENT_RTMP_BUILD_DIR)/rtmp_fetch_protocol.o \
        $(LIBMOMENT_RTMP_BUILD_DIR)/util_rtmp.o           \
                                                          \
        $(LIBMOMENT_RTMP_BUILD_DIR)/hmac/sha2.o           \
        $(LIBMOMENT_RTMP_BUILD_DIR)/hmac/hmac_sha2.o

MOMENT_RTMP_OBJECTS = \
        $(LIBMOMENT_RTMP_BUILD_DIR)/mod_rtmp.o

RTMPTOOL_OBJECTS = \
        $(LIBMOMENT_RTMP_BUILD_DIR)/rtmptool.o

LIBMOMENT_RTSP_OBJECTS =                            \
        $(LIBMOMENT_RTSP_BUILD_DIR)/rtp_port_pair.o \
        $(LIBMOMENT_RTSP_BUILD_DIR)/rtsp_client.o   \
        $(LIBMOMENT_RTSP_BUILD_DIR)/util_rtsp.o

MOMENT_RTSP_OBJECTS =                                     \
        $(LIBMOMENT_RTSP_BUILD_DIR)/rtsp_fetch_protocol.o \
        $(LIBMOMENT_RTSP_BUILD_DIR)/rtsp_service.o        \
        $(LIBMOMENT_RTSP_BUILD_DIR)/mod_rtsp.o

RTSPTOOL_OBJECTS = \
        $(LIBMOMENT_RTSP_BUILD_DIR)/rtsptool.o

MOMENT_HLS_OBJECTS =                          \
        $(MOMENT_HLS_BUILD_DIR)/mod_hls.o     \
        $(MOMENT_HLS_BUILD_DIR)/hls_service.o

MOMENT_VOD_OBJECTS = \
        $(MOMENT_VOD_BUILD_DIR)/mod_vod.o

MOMENT_AUTH_OBJECTS = \
        $(MOMENT_AUTH_BUILD_DIR)/mod_auth.o

MOMENT_NVR_OBJECTS =                                \
        $(MOMENT_NVR_BUILD_DIR)/types.o             \
        $(MOMENT_NVR_BUILD_DIR)/naming_scheme.o     \
        $(MOMENT_NVR_BUILD_DIR)/nvr_file_iterator.o \
        $(MOMENT_NVR_BUILD_DIR)/nvr_cleaner.o       \
        $(MOMENT_NVR_BUILD_DIR)/channel_recorder.o  \
        $(MOMENT_NVR_BUILD_DIR)/media_recorder.o    \
        $(MOMENT_NVR_BUILD_DIR)/media_reader.o      \
        $(MOMENT_NVR_BUILD_DIR)/nvr_reader.o        \
        $(MOMENT_NVR_BUILD_DIR)/camino_map.o        \
        $(MOMENT_NVR_BUILD_DIR)/camino_reader.o     \
        $(MOMENT_NVR_BUILD_DIR)/get_file_session.o  \
        $(MOMENT_NVR_BUILD_DIR)/moment_nvr_module.o \
        $(MOMENT_NVR_BUILD_DIR)/nvr_vod_service.o   \
        $(MOMENT_NVR_BUILD_DIR)/mod_nvr.o

MOMENT_THUMB_OBJECTS =                            \
        $(MOMENT_THUMB_BUILD_DIR)/thumb_service.o \
        $(MOMENT_THUMB_BUILD_DIR)/mod_thumb.o

MOMENT_TEST_OBJECTS =                          \
        $(MOMENT_TEST_BUILD_DIR)/moment_test.o \
        $(MOMENT_TEST_BUILD_DIR)/test_rtmp.o

$(LIBMARY_TARGET): $(LIBMARY_OBJECTS)
	$(AR_COMMAND)
$(TEST_LIBMARY_TARGET): $(TEST_LIBMARY_OBJECTS) \
                        $(LIBMARY_TARGET)
	$(CXX_LINK_COMMAND)
$(LIBMARY_BUILD_DIR)/test_libmary.stamp: $(TEST_LIBMARY_TARGET)
	@echo "Running test_libmary..."
	$(TEST_LIBMARY_TARGET)
	touch $(LIBMARY_BUILD_DIR)/test_libmary.stamp
LIBMARY_BUILD_MD5_DIR = $(LIBMARY_BUILD_DIR)/md5
$(LIBMARY_BUILD_DIR)/%.o: libmary/libmary/%.cpp | $(LIBMARY_BUILD_DIR) $(LIBMARY_BUILD_MD5_DIR)
	$(CXX_COMMAND) -I libmary
$(LIBMARY_BUILD_DIR)/%.o: libmary/libmary/%.c   | $(LIBMARY_BUILD_DIR) $(LIBMARY_BUILD_MD5_DIR)
	$(CC_COMMAND)  -I libmary
$(LIBMARY_BUILD_DIR):
	mkdir -p $(LIBMARY_BUILD_DIR)
$(LIBMARY_BUILD_MD5_DIR): | $(LIBMARY_BUILD_DIR)
	mkdir -p $(LIBMARY_BUILD_MD5_DIR)
-include $(LIBMARY_BUILD_MD5_DIR)/*.d
-include $(LIBMARY_BUILD_DIR)/*.d

$(PARGEN_TARGET): $(PARGEN_OBJECTS)
	$(AR_COMMAND)
$(PARGEN_BUILD_DIR)/%.o: pargen/pargen/%.cpp | $(PARGEN_BUILD_DIR)
	$(CXX_COMMAND) -I libmary -I pargen
$(PARGEN_BUILD_DIR):
	mkdir -p $(PARGEN_BUILD_DIR)
-include $(PARGEN_BUILD_DIR)/*.d

$(SCRUFFY_TARGET): $(SCRUFFY_OBJECTS)
	$(AR_COMMAND)
$(SCRUFFY_BUILD_DIR)/%.o: scruffy/scruffy/%.cpp | $(SCRUFFY_BUILD_DIR) $(SCRUFFY_GENFILES)
	$(CXX_COMMAND) -I libmary -I pargen -I scruffy
$(SCRUFFY_BUILD_DIR):
	mkdir -p $(SCRUFFY_BUILD_DIR)
-include $(SCRUFFY_BUILD_DIR)/*.d

$(MCONFIG_TARGET): $(MCONFIG_OBJECTS)
	$(AR_COMMAND)
$(MCONFIG_BUILD_DIR)/%.o: mconfig/mconfig/%.cpp | $(MCONFIG_BUILD_DIR) $(MCONFIG_GENFILES)
	$(CXX_COMMAND) -I libmary -I pargen -I scruffy -I mconfig
$(MCONFIG_BUILD_DIR):
	mkdir -p $(MCONFIG_BUILD_DIR)
-include $(MCONFIG_BUILD_DIR)/*.d

$(LIBMOMENT_TARGET): $(LIBMOMENT_OBJECTS)
	$(AR_COMMAND)
$(MOMENT_BUILD_DIR)/%.o: moment/moment/%.cpp | $(MOMENT_BUILD_DIR)
	$(CXX_COMMAND) -I libmary -I pargen -I scruffy -I mconfig -I moment -DLIBMOMENT_PREFIX="\"/opt/moment\""
$(MOMENT_BUILD_DIR):
	mkdir -p $(MOMENT_BUILD_DIR)
-include $(MOMENT_BUILD_DIR)/*.d

$(MOMENT_MOD_TEST_TARGET): $(MOMENT_MOD_TEST_OBJECTS)
	$(AR_COMMAND)

$(MOMENT_FILE_TARGET): $(MOMENT_FILE_OBJECTS)
	$(AR_COMMAND)

$(LIBMOMENT_INSTANCE_TARGET): $(LIBMOMENT_INSTANCE_OBJECTS)
	$(AR_COMMAND)

ifeq ($(TARGET_PLATFORM), macosx)
$(MOMENT_TARGET): $(MOMENT_OBJECTS)            \
                  $(LIBMOMENT_INSTANCE_TARGET) \
                  $(MOMENT_VOD_TARGET)         \
                  $(MOMENT_AUTH_TARGET)        \
                  $(MOMENT_NVR_TARGET)         \
                  $(MOMENT_MOD_TEST_TARGET)    \
                  $(MOMENT_FILE_TARGET)        \
                  $(MOMENT_HLS_TARGET)         \
                  $(MOMENT_RTMP_TARGET)        \
                  $(LIBMOMENT_RTMP_TARGET)     \
                  $(MOMENT_RTSP_TARGET)        \
                  $(LIBMOMENT_RTSP_TARGET)     \
                  $(LIBMOMENT_TARGET)          \
                  $(SCRUFFY_TARGET)            \
                  $(MCONFIG_TARGET)            \
                  $(PARGEN_TARGET)             \
                  $(LIBMARY_TARGET)
#                  $(MOMENT_THUMB_TARGET)
#                  libs/libav-11/libavcodec/libavcodec.a
#                  libs/libav-11/libavutil/libavutil.a
#                  libs/libav-11/libavresample/libavresample.a
	$(CXX_LINK_COMMAND) # -L libs/libav-11/libavcodec -L libs/libav-11/libavutil -lavcodec -lavutil
else
$(MOMENT_TARGET): $(MOMENT_OBJECTS)            \
                  $(LIBMOMENT_INSTANCE_TARGET) \
                  $(MOMENT_VOD_TARGET)         \
                  $(MOMENT_AUTH_TARGET)        \
                  $(MOMENT_NVR_TARGET)         \
                  $(MOMENT_MOD_TEST_TARGET)    \
                  $(MOMENT_FILE_TARGET)        \
                  $(MOMENT_HLS_TARGET)         \
                  $(MOMENT_RTMP_TARGET)        \
                  $(LIBMOMENT_RTMP_TARGET)     \
                  $(MOMENT_RTSP_TARGET)        \
                  $(LIBMOMENT_RTSP_TARGET)     \
                  $(LIBMOMENT_TARGET)          \
                  $(MCONFIG_TARGET)            \
                  $(SCRUFFY_TARGET)            \
                  $(PARGEN_TARGET)             \
                  $(LIBMARY_TARGET)
	$(CXX_LINK_COMMAND)
endif

$(HLSTOOL_TARGET): $(HLSTOOL_OBJECTS)  \
                   $(LIBMOMENT_TARGET) \
                   $(MCONFIG_TARGET)   \
                   $(SCRUFFY_TARGET)   \
                   $(PARGEN_TARGET)    \
                   $(LIBMARY_TARGET)
	$(CXX_LINK_COMMAND)

$(LIBMOMENT_RTMP_TARGET): $(LIBMOMENT_RTMP_OBJECTS)
	$(AR_COMMAND)
LIBMOMENT_RTMP_BUILD_HMAC_DIR = $(LIBMOMENT_RTMP_BUILD_DIR)/hmac
$(LIBMOMENT_RTMP_BUILD_DIR)/%.o: moment-rtmp/moment-rtmp/%.cpp | $(LIBMOMENT_RTMP_BUILD_DIR) $(LIBMOMENT_RTMP_BUILD_HMAC_DIR)
	$(CXX_COMMAND) -I libmary -I pargen -I scruffy -I mconfig -I moment -I moment-rtmp
$(LIBMOMENT_RTMP_BUILD_DIR)/%.o: moment-rtmp/moment-rtmp/%.c   | $(LIBMOMENT_RTMP_BUILD_DIR) $(LIBMOMENT_RTMP_BUILD_HMAC_DIR)
	$(CC_COMMAND)  -I libmary -I pargen -I scruffy -I mconfig -I moment -I moment-rtmp
$(LIBMOMENT_RTMP_BUILD_DIR):
	mkdir -p $(LIBMOMENT_RTMP_BUILD_DIR)
$(LIBMOMENT_RTMP_BUILD_HMAC_DIR): | $(LIBMOMENT_RTMP_BUILD_DIR)
	mkdir -p $(LIBMOMENT_RTMP_BUILD_HMAC_DIR)
-include $(LIBMOMENT_RTMP_BUILD_HMAC_DIR)/*.d
-include $(LIBMOMENT_RTMP_BUILD_DIR)/*.d

$(MOMENT_RTMP_TARGET): $(MOMENT_RTMP_OBJECTS)
	$(AR_COMMAND)

$(RTMPTOOL_TARGET): $(RTMPTOOL_OBJECTS)      \
                    $(LIBMOMENT_RTMP_TARGET) \
                    $(LIBMOMENT_TARGET)      \
                    $(MCONFIG_TARGET)        \
                    $(SCRUFFY_TARGET)        \
                    $(PARGEN_TARGET)         \
                    $(LIBMARY_TARGET)
	$(CXX_LINK_COMMAND)

$(LIBMOMENT_RTSP_TARGET): $(LIBMOMENT_RTSP_OBJECTS)
	$(AR_COMMAND)
$(LIBMOMENT_RTSP_BUILD_DIR)/%.o: moment-rtsp/moment-rtsp/%.cpp | $(LIBMOMENT_RTSP_BUILD_DIR)
	$(CXX_COMMAND) -I libmary -I pargen -I scruffy -I mconfig -I moment -I moment-rtsp
$(LIBMOMENT_RTSP_BUILD_DIR):
	mkdir -p $(LIBMOMENT_RTSP_BUILD_DIR)
-include $(LIBMOMENT_RTSP_BUILD_DIR)/*.d

$(MOMENT_RTSP_TARGET): $(MOMENT_RTSP_OBJECTS)
	$(AR_COMMAND)

$(RTSPTOOL_TARGET): $(RTSPTOOL_OBJECTS)      \
                    $(LIBMOMENT_RTSP_TARGET) \
                    $(LIBMOMENT_TARGET)      \
                    $(MCONFIG_TARGET)        \
                    $(SCRUFFY_TARGET)        \
                    $(PARGEN_TARGET)         \
                    $(LIBMARY_TARGET)
	$(CXX_LINK_COMMAND)

$(MOMENT_HLS_TARGET): $(MOMENT_HLS_OBJECTS)
	$(AR_COMMAND)
$(MOMENT_HLS_BUILD_DIR)/%.o: moment-hls/moment-hls/%.cpp | $(MOMENT_HLS_BUILD_DIR)
	$(CXX_COMMAND) -I libmary -I pargen -I scruffy -I mconfig -I moment -I moment-hls
$(MOMENT_HLS_BUILD_DIR):
	mkdir -p $(MOMENT_HLS_BUILD_DIR)
-include $(MOMENT_HLS_BUILD_DIR)/*.d

$(MOMENT_VOD_TARGET): $(MOMENT_VOD_OBJECTS)
	$(AR_COMMAND)
$(MOMENT_VOD_BUILD_DIR)/%.o: moment-vod/moment-vod/%.cpp | $(MOMENT_VOD_BUILD_DIR)
	$(CXX_COMMAND) -I libmary -I pargen -I scruffy -I mconfig -I moment -I moment-vod
$(MOMENT_VOD_BUILD_DIR):
	mkdir -p $(MOMENT_VOD_BUILD_DIR)
-include $(MOMENT_VOD_BUILD_DIR)/*.d

$(MOMENT_AUTH_TARGET): $(MOMENT_AUTH_OBJECTS)
	$(AR_COMMAND)
$(MOMENT_AUTH_BUILD_DIR)/%.o: moment-auth/moment-auth/%.cpp | $(MOMENT_AUTH_BUILD_DIR)
	$(CXX_COMMAND) -I libmary -I pargen -I scruffy -I mconfig -I moment -I moment-auth
$(MOMENT_AUTH_BUILD_DIR):
	mkdir -p $(MOMENT_AUTH_BUILD_DIR)
-include $(MOMENT_AUTH_BUILD_DIR)/*.d

$(MOMENT_NVR_TARGET): $(MOMENT_NVR_OBJECTS)
	$(AR_COMMAND)
$(MOMENT_NVR_BUILD_DIR)/%.o: moment-nvr/moment-nvr/%.cpp | $(MOMENT_NVR_BUILD_DIR)
	$(CXX_COMMAND) -I libmary -I pargen -I scruffy -I mconfig -I moment -I moment-nvr
$(MOMENT_NVR_BUILD_DIR):
	mkdir -p $(MOMENT_NVR_BUILD_DIR)
-include $(MOMENT_NVR_BUILD_DIR)/*.d

$(MOMENT_THUMB_TARGET): $(MOMENT_THUMB_OBJECTS)
	$(AR_COMMAND)
$(MOMENT_THUMB_BUILD_DIR)/%.o: moment-thumb/moment-thumb/%.cpp | $(MOMENT_THUMB_BUILD_DIR)
	$(CXX_COMMAND) -I libmary -I pargen -I scruffy -I mconfig -I moment -I moment-thumb -I libs/libav-11
$(MOMENT_THUMB_BUILD_DIR):
	mkdir -p $(MOMENT_THUMB_BUILD_DIR)
-include $(MOMENT_THUMB_BUILD_DIR)/*.d

$(MOMENT_TEST_TARGET): $(MOMENT_TEST_OBJECTS)       \
                       $(LIBMOMENT_INSTANCE_TARGET) \
                       $(MOMENT_VOD_TARGET)         \
                       $(MOMENT_AUTH_TARGET)        \
                       $(MOMENT_NVR_TARGET)         \
                       $(MOMENT_MOD_TEST_TARGET)    \
                       $(MOMENT_FILE_TARGET)        \
                       $(MOMENT_HLS_TARGET)         \
                       $(MOMENT_RTMP_TARGET)        \
                       $(LIBMOMENT_RTMP_TARGET)     \
                       $(MOMENT_RTSP_TARGET)        \
                       $(LIBMOMENT_RTSP_TARGET)     \
                       $(LIBMOMENT_TARGET)          \
                       $(MCONFIG_TARGET)            \
                       $(SCRUFFY_TARGET)            \
                       $(PARGEN_TARGET)             \
                       $(LIBMARY_TARGET)
	$(CXX_LINK_COMMAND)
$(MOMENT_TEST_BUILD_DIR)/%.o: moment-test/moment-test/%.cpp | $(MOMENT_TEST_BUILD_DIR)
	$(CXX_COMMAND) -I libmary -I pargen -I scruffy -I mconfig -I moment -I moment-rtmp -I moment-test
$(MOMENT_TEST_BUILD_DIR):
	mkdir -p $(MOMENT_TEST_BUILD_DIR)
-include $(MOMENT_TEST_BUILD_DIR)/*.d

test: $(TEST_LIBMARY_TARGET)
#	@echo "Running test_libmary..."
#	$(TEST_LIBMARY_TARGET)
#	touch $(LIBMARY_BUILD_DIR)/test_libmary.stamp

clean:
	rm -f  $(TESTS)

	rm -f  $(LIBMARY_TARGET) $(LIBMARY_OBJECTS)
	rm -f  $(TEST_LIBMARY_TARGET) $(TEST_LIBMARY_OBJECTS)
	rm -f  $(LIBMARY_BUILD_MD5_DIR)/*.d
	-rmdir $(LIBMARY_BUILD_MD5_DIR)
	rm -f  $(LIBMARY_BUILD_DIR)/*.d
	-rmdir $(LIBMARY_BUILD_DIR)

	rm -f  $(PARGEN_TARGET) $(PARGEN_OBJECTS)
	rm -f  $(PARGEN_BUILD_DIR)/*.d
	-rmdir $(PARGEN_BUILD_DIR)

	rm -f  $(SCRUFFY_TARGET) $(SCRUFFY_OBJECTS) $(SCRUFFY_GENFILES)
	rm -f  $(SCRUFFY_BUILD_DIR)/*.d
	-rmdir $(SCRUFFY_BUILD_DIR)

	rm -f  $(MCONFIG_TARGET) $(MCONFIG_OBJECTS) $(MCONFIG_GENFILES)
	rm -f  $(MCONFIG_BUILD_DIR)/*.d
	-rmdir $(MCONFIG_BUILD_DIR)

	rm -f  $(LIBMOMENT_TARGET) $(LIBMOMENT_OBJECTS)
	rm -f  $(MOMENT_MOD_TEST_TARGET) $(MOMENT_MOD_TEST_OBJECTS)
	rm -f  $(MOMENT_FILE_TARGET) $(MOMENT_FILE_OBJECTS)
	rm -f  $(LIBMOMENT_INSTANCE_TARGET) $(LIBMOMENT_INSTANCE_OBJECTS)
	rm -f  $(MOMENT_TARGET) $(MOMENT_OBJECTS)
	rm -f  $(HLSTOOL_TARGET) $(HLSTOOL_OBJECTS)
	rm -f  $(MOMENT_BUILD_DIR)/*.d
	-rmdir $(MOMENT_BUILD_DIR)

	rm -f  $(LIBMOMENT_RTMP_TARGET) $(LIBMOMENT_RTMP_OBJECTS)
	rm -f  $(MOMENT_RTMP_TARGET) $(MOMENT_RTMP_OBJECTS)
	rm -f  $(RTMPTOOL_TARGET) $(RTMPTOOL_OBJECTS)
	rm -f  $(LIBMOMENT_RTMP_BUILD_HMAC_DIR)/*.d
	-rmdir $(LIBMOMENT_RTMP_BUILD_HMAC_DIR)
	rm -f  $(LIBMOMENT_RTMP_BUILD_DIR)/*.d
	-rmdir $(LIBMOMENT_RTMP_BUILD_DIR)

	rm -f  $(LIBMOMENT_RTSP_TARGET) $(LIBMOMENT_RTSP_OBJECTS)
	rm -f  $(MOMENT_RTSP_TARGET) $(MOMENT_RTSP_OBJECTS)
	rm -f  $(RTSPTOOL_TARGET) $(RTSPTOOL_OBJECTS)
	rm -f  $(LIBMOMENT_RTSP_BUILD_DIR)/*.d
	-rmdir $(LIBMOMENT_RTSP_BUILD_DIR)

	rm -f  $(MOMENT_HLS_TARGET) $(MOMENT_HLS_OBJECTS)
	rm -f  $(MOMENT_HLS_BUILD_DIR)/*.d
	-rmdir $(MOMENT_HLS_BUILD_DIR)

	rm -f  $(MOMENT_VOD_TARGET) $(MOMENT_VOD_OBJECTS)
	rm -f  $(MOMENT_VOD_BUILD_DIR)/*.d
	-rmdir $(MOMENT_VOD_BUILD_DIR)

	rm -f  $(MOMENT_AUTH_TARGET) $(MOMENT_AUTH_OBJECTS)
	rm -f  $(MOMENT_AUTH_BUILD_DIR)/*.d
	-rmdir $(MOMENT_AUTH_BUILD_DIR)

	rm -f  $(MOMENT_NVR_TARGET) $(MOMENT_NVR_OBJECTS)
	rm -f  $(MOMENT_NVR_BUILD_DIR)/*.d
	-rmdir $(MOMENT_NVR_BUILD_DIR)

	rm -f  $(MOMENT_THUMB_TARGET) $(MOMENT_THUMB_OBJECTS)
	rm -f  $(MOMENT_THUMB_BUILD_DIR)/*.d
	-rmdir $(MOMENT_THUMB_BUILD_DIR)

	rm -f  $(MOMENT_TEST_TARGET) $(MOMENT_TEST_OBJECTS)
	rm -f  $(MOMENT_TEST_BUILD_DIR)/*.d
	-rmdir $(MOMENT_TEST_BUILD_DIR)

	-rmdir $(TARGET_BUILD_DIR)
	-rmdir build

