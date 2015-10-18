#include <libmary/libmary.h>


namespace libMary_Test {

using namespace M;

static void verifyFileContent (Vfs         * const mt_nonnull vfs,
                               ConstMemory   const path,
                               ConstMemory   const expected_content)
{
    Buffer buf;
    buf.allocate (expected_content.len());

    StRef<Vfs::VfsFile> const file = mt_throws vfs->openFile (path, /*open_flags=*/ 0, FileAccessMode::ReadOnly);
    if (!file) {
        logE_ (_func, "openFile(\"", path, "\") failed: ", exc->toString());
        assert_hard (0 && "could not open file");
    }

    Size nread = 0;
    if (mt_throws file->getFile()->readFull (buf.mem, &nread) != IoResult::Normal) {
        logE_ (_func, "readFull(\"", path, "\" failed: ", exc->toString());
        assert_hard (0 && "could not read file");
    }

    assert_hard (equal (buf.mem.region (0, nread), expected_content));
}

static StRef<String> makeTestLogFilename (unsigned          const idx,
                                          struct tm const * const mt_nonnull localtime)
{
    Format idx_fmt  (/*num_base=*/   10,
                     /*min_digits=*/  4,
                     /*precision=*/  (unsigned) -1);

    Format year_fmt (/*num_base=*/   10,
                     /*min_digits=*/  4,
                     /*precision=*/  (unsigned) -1);

    Format mmdd_fmt (/*num_base=*/   10,
                     /*min_digits=*/  2,
                     /*precision=*/  (unsigned) -1);

    return makeString ("test",
                       "_", idx_fmt,  idx,
                       "_", year_fmt, localtime->tm_year + 1900,
                       "-", mmdd_fmt, localtime->tm_mon + 1,
                       "-",           localtime->tm_mday,
                       ".log");
}

void testLogRotateStream ()
{
    ConstMemory const dir_name = "libmary_test__log_rotate_stream";

    StRef<Vfs> const vfs = Vfs::createDefaultLocalVfs (dir_name);

    if (! mt_throws vfs->removeDirectoryWithAllContents (ConstMemory())) {
        logE_ (_func, "removeDirectoryWithAllContents(\"", dir_name, "\") failed: ", exc->toString());
        assert_hard (0 && "could not remove directory");
    }

    if (! mt_throws vfs->createDirectory (ConstMemory())) {
        logE_ (_func, "createDirectory(\"", dir_name, "\") failed: ", exc->toString());
        assert_hard (0 && "could not create directory");
    }

    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    struct tm const saved_localtime = tlocal->localtime;

    {
        Ref<LogRotateStream> const stream = grabNewObject <LogRotateStream> ();
        stream->init (dir_name,
                      "test",
                      ".log",
                      /*max_days=*/ 3,
                      /*max_total_size=*/ 4096,
                      /*max_file_size=*/  4096);

        struct tm const localtime_1 = tlocal->localtime;
        stream->print ("#1\n");
        stream->flush();
        verifyFileContent (vfs, makeTestLogFilename (1, &tlocal->localtime), "#1\n");

        tlocal->localtime.tm_mday = (tlocal->localtime.tm_mday + 1) % 32;
        stream->print ("#2\n");
        stream->flush();
        verifyFileContent (vfs, makeTestLogFilename (2, &tlocal->localtime), "#2\n");

        tlocal->localtime.tm_mday = (tlocal->localtime.tm_mday + 1) % 32;
        stream->print ("#3\n");
        stream->flush();
        verifyFileContent (vfs, makeTestLogFilename (3, &tlocal->localtime), "#3\n");
        stream->print ("#3-2\n");
        stream->flush();
        verifyFileContent (vfs, makeTestLogFilename (3, &tlocal->localtime), "#3\n#3-2\n");

        tlocal->localtime.tm_mday = (tlocal->localtime.tm_mday + 1) % 32;
        stream->print ("#4\n");
        stream->flush();
        verifyFileContent (vfs, makeTestLogFilename (4, &tlocal->localtime), "#4\n");

        tlocal->localtime.tm_mday = (tlocal->localtime.tm_mday + 1) % 32;
        stream->print ("#5\n");
        stream->flush();
        verifyFileContent (vfs, makeTestLogFilename (5, &tlocal->localtime), "#5\n");

        assert_hard (!vfs->openFile (makeTestLogFilename (1, &localtime_1), /*open_flags=*/ 0, FileAccessMode::ReadOnly));

        // TODO Write more and verify that trailing files are removed.
        //      "File exists" check is needed. Test for ENOENT and return
        //      a standard Exception in NativeFile for both POSIX and Win32..
    }

    tlocal->localtime = saved_localtime;
}

}

