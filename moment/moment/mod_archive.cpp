/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/module_init.h>

#include <moment/libmoment.h>


// TODO These header macros are the same as in rtmpt_server.cpp
#define MOMENT_ARCHIVE__HEADERS_DATE \
	Byte date_buf [unixtimeToString_BufSize]; \
	Size const date_len = unixtimeToString (Memory::forObject (date_buf), getUnixtime());

#define MOMENT_ARCHIVE__COMMON_HEADERS \
	"Server: Moment/1.0\r\n" \
	"Date: ", ConstMemory (date_buf, date_len), "\r\n" \
	"Connection: Keep-Alive\r\n" \
	"Cache-Control: no-cache\r\n"

#define MOMENT_ARCHIVE__OK_HEADERS(mime_type, content_length) \
	"HTTP/1.1 200 OK\r\n" \
	MOMENT_ARCHIVE__COMMON_HEADERS \
	"Content-Type: ", (mime_type), "\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_ARCHIVE__404_HEADERS(content_length) \
	"HTTP/1.1 404 Not found\r\n" \
	MOMENT_ARCHIVE__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_ARCHIVE__400_HEADERS(content_length) \
	"HTTP/1.1 400 Bad Request\r\n" \
	MOMENT_ARCHIVE__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_ARCHIVE__500_HEADERS(content_length) \
	"HTTP/1.1 500 Internal Server Error\r\n" \
	MOMENT_ARCHIVE__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"


namespace Moment {

static PagePool *page_pool = NULL;
static ConstMemory archive_prefix = "/home/erdizz/archive" /* TODO Config parameter */;

class CandidateFileEntry : public StReferenced
{
public:
    StRef<String> filename;
    Time start_unixtime;
};

typedef Map< StRef<CandidateFileEntry>,
	     MemberExtractor< CandidateFileEntry,
			      StRef<String>,
			      &CandidateFileEntry::filename,
			      Memory,
			      AccessorExtractor< String,
						 Memory,
						 &String::mem > >,
	     MemoryComparator<> >
	CandidateFileMap;

static Result scanFlvFile (ConstMemory   const filename,
			   Time          const file_unixtime,
			   Time          const timestamp_offs,
			   Time          const start_unixtime,
			   Time          const end_unixtime,
			   PagePool    * const page_pool,
			   PagePool::PageListInfo * const mt_nonnull ret_page_list,
			   bool        * const mt_nonnull ret_last_file,
			   Uint32      * const mt_nonnull ret_last_timestamp,
			   Size        * const mt_nonnull ret_data_len)
{
    *ret_last_file = false;
    *ret_last_timestamp = timestamp_offs;
    *ret_data_len = 0;

    NativeFile file (NULL /* embed_container */);
    if (!file.open (filename, 0 /* open_flags */, File::AccessMode::ReadOnly)) {
	logE_ (_func, "Could not open file ", filename, ": ", exc->toString());
	return Result::Failure;
    }

    Size nread;

    Byte flv_header [9];
    Byte prv_tag_size_buf [4];
    Byte flv_tag [11];

    Byte data_buf [1 << 16 /* 64 Kb */];

    if (file.readFull (Memory::forObject (flv_header), &nread) != IoResult::Normal) {
	logE_ (_func, "Could not read FLV header");
	return Result::Failure;
    }
    if (nread != sizeof (flv_header)) {
	logE_ (_func, "FLV header is too short");
	return Result::Failure;
    }

    Uint32 first_timestamp = 0;
    bool got_first_timestamp = false;
    for (Count tag_index = 0; ; ++tag_index) {
	if (file.readFull (Memory::forObject (prv_tag_size_buf), &nread) != IoResult::Normal) {
	    // TODO Distinguish error/eof.
	    logD_ (_func, "#", tag_index, " no prv tag length");
	    break;
	}
	if (nread != sizeof (prv_tag_size_buf)) {
	    logE_ (_func, "#", tag_index, " incomplete prv tag size");
	    break;
	}

	if (file.readFull (Memory::forObject (flv_tag), &nread) != IoResult::Normal) {
//	    logD_ (_func, "#", tag_index, " could not read flv tag header");
	    logD_ (_func, "#", tag_index, " no more tags");
	    break;
	}
	if (nread != sizeof (flv_tag)) {
	    logE_ (_func, "#", tag_index, " incomplete flv tag header");
	    break;
	}

	Byte const tag_type = flv_tag [0] & 0x1f;

	Uint32 const data_size = ((Uint32) flv_tag [1] << 16) |
				 ((Uint32) flv_tag [2] <<  8) |
				 ((Uint32) flv_tag [3] <<  0);

	Uint32 const timestamp = ((Uint32) flv_tag [4] << 16) |
				 ((Uint32) flv_tag [5] <<  8) |
				 ((Uint32) flv_tag [6] <<  0) |
				 ((Uint32) flv_tag [7] << 24);

//	logD_ (_func, "timestamp: 0x", fmt_hex, timestamp, fmt_def, ", type: ", (Uint32) tag_type, ", data_size: ", data_size);

	if (!got_first_timestamp) {
	    if (timestamp != 0 && timestamp != (Uint32) -1) {
		first_timestamp = timestamp;
		got_first_timestamp = true;
	    }
	}

	{
	    Size total_read = 0;
	    while (total_read < data_size) {
		Size toread = sizeof (data_buf);
		if (total_read + toread > data_size) {
		    toread = data_size - total_read;
		    assert (toread <= sizeof (data_buf));
		}

		if (file.readFull (Memory (data_buf, toread), &nread) != IoResult::Normal) {
		    logE_ (_func, "#", tag_index, " incomplete flv tag body");
		    break;
		}

		total_read += nread;
	    }
	}

	// Frame timestamp in seconds.
	Uint32 const ts_sec = timestamp / 1000;
	Uint32 const frame_unixtime = ts_sec + file_unixtime;

	if (frame_unixtime >= start_unixtime &&
	    frame_unixtime <= end_unixtime)
	{
	    Uint32 new_timestamp = timestamp_offs;
	    if (got_first_timestamp)
		new_timestamp += timestamp - first_timestamp;

	    flv_tag [4] = (new_timestamp >> 16) & 0xff;
	    flv_tag [5] = (new_timestamp >>  8) & 0xff;
	    flv_tag [6] = (new_timestamp >>  0) & 0xff;
	    flv_tag [7] = (new_timestamp >> 24) & 0xff;

	    page_pool->getFillPages (ret_page_list, ConstMemory::forObject (prv_tag_size_buf));
	    page_pool->getFillPages (ret_page_list, ConstMemory::forObject (flv_tag));
	    page_pool->getFillPages (ret_page_list, ConstMemory (data_buf, data_size));

	    *ret_last_timestamp = new_timestamp;
	    *ret_data_len += sizeof (prv_tag_size_buf) + sizeof (flv_tag) + data_size;

//	    logD_ (_func, "frame_unixtime: ", frame_unixtime, ", new_timestamp 0x", fmt_hex, new_timestamp, " - ACCEPTED");
	} else {
//	    logD_ (_func, "frame_unixtime: ", frame_unixtime);
	}

	if (frame_unixtime > end_unixtime) {
	    *ret_last_file = true;
	    break;
	}
    }

    if (!file.close ())
	logE_ (_func, "Could not close file ", filename, " properly: ", exc->toString());

    return Result::Success;
}

static Result videoHttpRequest (HttpRequest  * const mt_nonnull req,
                                HttpService::HttpConnectionInfo * const mt_nonnull /* conn_info */,
				Sender       * const mt_nonnull conn_sender,
				Memory         const /* msg_body */,
				void        ** const mt_nonnull /* ret_msg_data */,
				void         * const cb_data)
{
  {
    logD_ (_func_);

    logD_ (_func, "num path elems: ", req->getNumPathElems());
    if (req->getNumPathElems() < 2) {
	logE_ (_func, "archive name not specified");
	goto _not_found;
    }

    ConstMemory archive_name = req->getPath (1);

    if (archive_name.len() >= 4 &&
	equal (archive_name.region (archive_name.len() - 4), ".flv"))
    {
	archive_name = archive_name.region (0, archive_name.len() - 4);
    }

    logD_ (_func, "archive_name: ", archive_name);

    Uint64 start_time = 0;
    {
	ConstMemory const start_mem = req->getParameter ("start");
	if (start_mem.mem()) {
	    if (!strToUint64_safe (start_mem, &start_time, 10)) {
		logE_ (_func, "bad value for \"start\": ", start_mem);
		goto _bad_request;
	    }
	}
    }

    Uint64 duration = 60 /* TODO default_duration config parameter */;
    {
	ConstMemory const duration_mem = req->getParameter ("duration");
	if (duration_mem.mem()) {
	    if (!strToUint64_safe (duration_mem, &duration, 10)) {
		logE_ (_func, "bad value for \"duration\": ", duration_mem);
		goto _bad_request;
	    }
	}
    }

    logD_ (_func, "start: ", start_time);
    logD_ (_func, "duration: ", duration);

    struct tm start_tm;
    splitTime (start_time, &start_tm);

    struct tm end_tm;
    splitTime (start_time + duration, &end_tm);

    logD_ (_func, "start year:   ", start_tm.tm_year + 1900);
    logD_ (_func, "start month:  ", start_tm.tm_mon + 1);
    logD_ (_func, "start day:    ", start_tm.tm_mday);
    logD_ (_func, "start hour:   ", start_tm.tm_hour);
    logD_ (_func, "start minute: ", start_tm.tm_min);

    logD_ (_func, "end_year:   ", end_tm.tm_year + 1900);
    logD_ (_func, "end month:  ", end_tm.tm_mon + 1);
    logD_ (_func, "end day:    ", end_tm.tm_mday);
    logD_ (_func, "end hour:   ", end_tm.tm_hour);
    logD_ (_func, "end minute: ", end_tm.tm_min);

    StRef<Vfs> const vfs = Vfs::createDefaultLocalVfs (archive_prefix);
    if (!vfs) {
	logE_ (_func, "Vfs::createDefaultLocalVfs() failed");
	goto _internal_error;
    }

    StRef<Vfs::VfsDirectory> const dir = vfs->openDirectory (archive_name);
    if (!dir) {
	logE_ (_func, "Could not open archive directory");
	goto _not_found;
    }

    CandidateFileMap file_map;
    for (;;) {
	StRef<String> year_dir_entry;
	if (!dir->getNextEntry (year_dir_entry)) {
	    logE_ (_func, "dir->getNextEntry() failed");
	    goto _internal_error;
	}
	if (!year_dir_entry)
	    break;

	if (equal (year_dir_entry->mem(), ".") || equal (year_dir_entry->mem(), ".."))
	    continue;

	logD_ (_func, "year dir: ", year_dir_entry->mem());

	Uint32 year;
	if (!strToUint32_safe (year_dir_entry->mem(), &year, 10)) {
	    logW_ (_func, "Could not  parse year: ", year_dir_entry->mem());
	    continue;
	}

	logD_ (_func, "year: ", year);

	if (year < (Uint32) start_tm.tm_year + 1900 ||
	    year > (Uint32) end_tm.tm_year + 1900)
	{
	    logD_ (_func, "Year not in requested interval");
	    continue;
	}

	logD_ (_func, "Scanning year ", year);

	StRef<String> const year_dir_path = makeString (archive_name, "/", year_dir_entry->mem());
	StRef<Vfs::VfsDirectory> const year_dir = vfs->openDirectory (year_dir_path->mem());
	if (!year_dir) {
	    logE_ (_func, "Could not open year directory ", year_dir_path->mem());
	    goto _internal_error;
	}

#if 0
	StRef<String> const stat_path = makeString (archive_prefix, "/", archive_name, "/", dir_entry->mem());

	Ref<Vfs::FileStat> const stat_data = vfs->stat (dir_entry->mem());
	if (!stat_data) {
	    logE_ (_func, "Could not stat ", stat_path);
	}
#endif

	for (;;) {
	    StRef<String> month_dir_entry;
	    if (!year_dir->getNextEntry (month_dir_entry)) {
		logE_ (_func, "dir->getNextEntry() failed");
		goto _internal_error;
	    }
	    if (!month_dir_entry)
		break;

	    if (equal (month_dir_entry->mem(), ".") || equal (month_dir_entry->mem(), ".."))
		continue;

	    logD_ (_func, "month dir: ", month_dir_entry->mem());

	    Uint32 month;
	    if (!strToUint32_safe (month_dir_entry->mem(), &month, 10)) {
		logW_ (_func, "Could not parse month: ", month_dir_entry->mem());
		continue;
	    }

	    logD_ (_func, "month: ", month);

	    if (((Uint32) start_tm.tm_year + 1900 == year
		 && month < (Uint32) start_tm.tm_mon + 1)
		||
		((Uint32) end_tm.tm_year + 1900 == year
		 && month > (Uint32) end_tm.tm_mon + 1))
	    {
		logD_ (_func, "Month not in requested interval");
		continue;
	    }

	    logD_ (_func, "Scanning month: ", month);

	    StRef<String> const month_dir_path = makeString (year_dir_path->mem(), "/", month_dir_entry->mem());
	    StRef<Vfs::VfsDirectory> const month_dir = vfs->openDirectory (month_dir_path->mem());
	    if (!month_dir) {
		logE_ (_func, "Could not open month directory ", month_dir_path->mem());
		goto _internal_error;
	    }

	    for (;;) {
		StRef<String> day_dir_entry;
		if (!month_dir->getNextEntry (day_dir_entry)) {
		    logE_ (_func, "dir->getNextEntry() failed");
		    goto _internal_error;
		}
		if (!day_dir_entry)
		    break;

		if (equal (day_dir_entry->mem(), ".") || equal (day_dir_entry->mem(), ".."))
		    continue;

		logD_ (_func, "day dir: ", day_dir_entry->mem());

		Uint32 day;
		if (!strToUint32_safe (day_dir_entry->mem(), &day, 10)) {
		    logW_ (_func, "Could not parse day: ", day_dir_entry->mem());
		    continue;
		}

		logD_ (_func, "day: ", day);

		if (((Uint32) start_tm.tm_year + 1900 == year
		     && (Uint32) start_tm.tm_mon + 1 == month
		     && day < (Uint32) start_tm.tm_mday)
		    ||
		    ((Uint32) end_tm.tm_year + 1900 == year
		     && (Uint32) end_tm.tm_mon + 1 == month
		     && day > (Uint32) end_tm.tm_mday))
		{
		    logD_ (_func, "Day not in requested interval");
		    continue;
		}

		StRef<String> const day_dir_path = makeString (month_dir_path->mem(), "/", day_dir_entry->mem());
		StRef<Vfs::VfsDirectory> const day_dir = vfs->openDirectory (day_dir_path->mem());
		if (!day_dir) {
		    logE_ (_func, "Could not open day directory ", day_dir_path->mem());
		    goto _internal_error;
		}

		for (;;) {
		    StRef<String> hour_dir_entry;
		    if (!day_dir->getNextEntry (hour_dir_entry)) {
			logE_ (_func, "dir->getNextEntry() failed");
			goto _internal_error;
		    }
		    if (!hour_dir_entry)
			break;

		    if (equal (hour_dir_entry->mem(), ".") || equal (hour_dir_entry->mem(), ".."))
			continue;

		    logD_ (_func, "hour dir: ", hour_dir_entry->mem());

		    Uint32 hour;
		    if (!strToUint32_safe (hour_dir_entry->mem(), &hour, 10)) {
			logW_ (_func, "Could not parse hour: ", hour_dir_entry->mem());
			continue;
		    }

		    logD_ (_func, "hour: ", hour);

		    if (((Uint32) start_tm.tm_year + 1900 == year
			 && (Uint32) start_tm.tm_mon + 1 == month
			 && (Uint32) start_tm.tm_mday == day
			 && hour < (Uint32) start_tm.tm_hour)
			||
			((Uint32) end_tm.tm_year + 1900 == year
			 && (Uint32) end_tm.tm_mon + 1 == month
			 && (Uint32) end_tm.tm_mday == day
			 && hour > (Uint32) end_tm.tm_hour))
		    {
			logD_ (_func, "Hour not in requested interval");
			continue;
		    }

		    StRef<String> const hour_dir_path = makeString (day_dir_path->mem(), "/", hour_dir_entry->mem());
		    StRef<Vfs::VfsDirectory> const hour_dir = vfs->openDirectory (hour_dir_path->mem());
		    if (!hour_dir) {
			logE_ (_func, "Could not open hour directory ", hour_dir_path->mem());
			goto _internal_error;
		    }

		    for (;;) {
			StRef<String> minute_dir_entry;
			if (!hour_dir->getNextEntry (minute_dir_entry)) {
			    logE_ (_func, "dir->getNextEntry() failed");
			    goto _internal_error;
			}
			if (!minute_dir_entry)
			    break;

			if (equal (minute_dir_entry->mem(), ".") || equal (minute_dir_entry->mem(), ".."))
			    continue;

			logD_ (_func, "minute entry: ", minute_dir_entry->mem());

			Uint32 minute;
			if (!strToUint32 (minute_dir_entry->mem(), &minute, NULL, 10)) {
			    logW_ (_func, "Could not parse minute: ", minute_dir_entry->mem());
			    continue;
			}

			logD_ (_func, "minute: ", minute);

			if (((Uint32) start_tm.tm_year + 1900 == year
			     && (Uint32) start_tm.tm_mon + 1 == month
			     && (Uint32) start_tm.tm_mday == day
			     && (Uint32) start_tm.tm_hour == hour
			     && minute + 10 /* TODO Hard-coded file duration */ < (Uint32) start_tm.tm_min)
			    ||
			    ((Uint32) end_tm.tm_year + 1900 == year
			     && (Uint32) end_tm.tm_mon + 1 == month
			     && (Uint32) end_tm.tm_mday == day
			     && (Uint32) end_tm.tm_hour == hour
			     && minute > (Uint32) end_tm.tm_min))
			{
			    logD_ (_func, "Minute not in requested interval");
			    continue;
			}

			StRef<String> const flv_file_path = makeString (hour_dir_path->mem(), "/", minute_dir_entry->mem());
			logD_ (_func, "--- FILE TO CONSIDER: ", flv_file_path);

			StRef<CandidateFileEntry> const file_entry = st_grab (new (std::nothrow) CandidateFileEntry);
			file_entry->filename = flv_file_path;
//			    file_entry->start_unixtime = end_tm.tm_year;

			struct tm file_tm;
			memset (&file_tm, 0, sizeof (file_tm));
			file_tm.tm_year = year - 1900;
			file_tm.tm_mon = month - 1;
			file_tm.tm_mday = day;
			file_tm.tm_hour = hour;
			file_tm.tm_min = minute;
			file_tm.tm_sec = 0;
			file_tm.tm_isdst = 0;

			time_t const file_start_time = mktime (&file_tm);
			logD_ (_func, "--- FILE START_TIME: ", file_start_time);

			file_entry->start_unixtime = (Time) file_start_time;

			file_map.add (file_entry);
		    } // for (;;) minutes
		} // for (;;) hours
	    } // for (;;) days
	} // for (;;) months
    } // for (;;) years

    if (file_map.isEmpty()) {
	logD_ (_func, "No matching archive records found");
	goto _not_found;
    }

    logD_ (_func, "Scanning matching recordings");

    PagePool::PageListInfo page_list;
    Size total_data_len = 0;

    static Byte const flv_header [] = {
	0x46, // 'F'
	0x4c, // 'L'
	0x56, // 'V'

	0x01, // FLV version 1
	0x05, // Audio and video tags are present

	// Data offset
	0x0,
	0x0,
	0x0,
	0x9,
    };

    page_pool->getFillPages (&page_list, ConstMemory::forObject (flv_header));
    total_data_len += sizeof (flv_header);

    Uint32 timestamp_offs = 0;
    CandidateFileMap::DataIterator file_iter = file_map.createDataIterator();
    while (!file_iter.done()) {
	StRef<CandidateFileEntry> const &file_entry = file_iter.next();
	logD_ (_func, "Candidate file: ", file_entry->start_unixtime, " ", file_entry->filename);

	bool last_file;
	Size data_len;
	if (!scanFlvFile (makeString (archive_prefix, "/", file_entry->filename->mem())->mem(),
			  file_entry->start_unixtime,
			  timestamp_offs,
			  start_time,
			  start_time + duration,
			  page_pool,
			  &page_list,
			  &last_file,
			  &timestamp_offs,
			  &data_len))
	{
	    logE_ (_func, "scanFlvFile() failed for file ", file_entry->filename);
	}

	total_data_len += data_len;

	if (last_file)
	    break;
    }

    logD_ (_func, "total_data_len: ", total_data_len);

    {
	MOMENT_ARCHIVE__HEADERS_DATE
	conn_sender->send (
		page_pool,
		false /* do_flush */,
		MOMENT_ARCHIVE__OK_HEADERS ("video/x-flv", total_data_len),
		"\r\n");

	conn_sender->sendPages (page_pool, &page_list, /*msg_offs=*/ 0, page_list.data_len, true /* do_flush */);

	if (!req->getKeepalive())
	    conn_sender->closeAfterFlush();
    }

    return Result::Success;
  }

_not_found:
    {
	MOMENT_ARCHIVE__HEADERS_DATE
	ConstMemory const reply_body = "404 Not Found";
	conn_sender->send (
		page_pool,
		true /* do_flush */,
		MOMENT_ARCHIVE__404_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);
	if (!req->getKeepalive())
	    conn_sender->closeAfterFlush();
    }

    return Result::Success;

_bad_request:
    {
	MOMENT_ARCHIVE__HEADERS_DATE
	ConstMemory const reply_body = "400 Bad Request";
	conn_sender->send (
		page_pool,
		true /* do_flush */,
		MOMENT_ARCHIVE__400_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);
	if (!req->getKeepalive())
	    conn_sender->closeAfterFlush();
    }

    return Result::Success;

_internal_error:
    {
	MOMENT_ARCHIVE__HEADERS_DATE
	ConstMemory const reply_body = "500 Internal Server Error";
	conn_sender->send (
		page_pool,
		true /* do_flush */,
		MOMENT_ARCHIVE__500_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);
	if (!req->getKeepalive())
	    conn_sender->closeAfterFlush();
    }

    return Result::Success;

}

static HttpService::HttpHandler const video_http_handler = {
    videoHttpRequest,
    NULL /* httpMessageBody */
};

static void momentArchiveInit ()
{
    logI_ (_func, "Initializing mod_archive");

    MomentServer * const moment = MomentServer::getInstance();
    HttpService * const http_service = moment->getHttpService();

    page_pool = moment->getPagePool();

    http_service->addHttpHandler (
	    CbDesc<HttpService::HttpHandler> (&video_http_handler, NULL, NULL),
	    "archive");
}

static void momentArchiveUnload ()
{
    logI_ (_func, "Unloading mod_archive");
}

}


extern "C" {

bool M::libMary_moduleInit ()
{
    Moment::momentArchiveInit ();
    return true;
}

void M::libMary_moduleUnload ()
{
    Moment::momentArchiveUnload ();
}

}

