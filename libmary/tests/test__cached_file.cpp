#include <libmary/libmary.h>


using namespace M;

static Ref<PagePool> createPagePool (Size const page_size)
{
    Ref<PagePool> const page_pool = grab (new (std::nothrow) PagePool (NULL /* embed_container */));
    page_pool->init (page_size,
                     0    /* min_pages */,
                     true /* delete_immediately */,
                     NULL /* timers */,
                     0    /* delete_interval_microsec */,
                     0    /* delete_bulk_size */);
    return page_pool;
}

static void doTestOne (Size   const page_size,
                       Size   const max_pages,
                       File * const mt_nonnull orig_file)
{
    Ref<PagePool> const page_pool = createPagePool (page_size);
    CachedFile file;
    file.init (orig_file, page_pool, max_pages);
    {
        Byte b;
        Size nread;
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b == 0);
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b == 1);
        assert_hard (file.readFull (Memory::forObject (b), NULL) == IoResult::Normal);
        assert_hard (b == 2);
        assert_hard (file.readFull (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b == 3);
        assert_hard (file.read (Memory::forObject (b), NULL) == IoResult::Normal);
        assert_hard (b == 4);
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b == 5);
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Eof);
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Eof);
        assert_hard (file.readFull (Memory::forObject (b), NULL) == IoResult::Eof);

        assert_hard (file.seek (-3, SeekOrigin::Cur));
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b == 3);
        assert_hard (file.seek (1, SeekOrigin::Cur));
        assert_hard (file.readFull (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b == 5);
        assert_hard (file.seek (1, SeekOrigin::Beg));
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b == 1);

        assert_hard (file.seek (0, SeekOrigin::Beg));
        assert_hard (file.read (Memory::forObject (b), NULL) == IoResult::Normal);
        assert_hard (b == 0);
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b == 1);
        assert_hard (file.readFull (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b == 2);
        assert_hard (file.read (Memory::forObject (b), NULL) == IoResult::Normal);
        assert_hard (b == 3);
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b == 4);
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b == 5);
        assert_hard (file.readFull (Memory::forObject (b), &nread) == IoResult::Eof);
        assert_hard (file.read (Memory::forObject (b), NULL) == IoResult::Eof);
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Eof);
    }
    {
        Byte b [2];
        Size nread;
        assert_hard (file.seekBeg (0));
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 2 && b[0] == 0 && b [1] == 1);
        assert_hard (file.seek (3, SeekOrigin::Beg));
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 2 && b[0] == 3 && b [1] == 4);
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Normal);
        assert_hard (nread == 1 && b[0] == 5);
        assert_hard (file.read (Memory::forObject (b), NULL) == IoResult::Eof);
        assert_hard (file.read (Memory::forObject (b), &nread) == IoResult::Eof);
        assert_hard (file.readFull (Memory::forObject (b), &nread) == IoResult::Eof);
    }
    for (unsigned i = 6; i < 32; ++i) {
        Byte b [i];
        Size nread;
        assert_hard (file.seek (0, SeekOrigin::Beg));
        assert_hard (file.read (Memory (b, i), &nread) == IoResult::Normal);
        assert_hard (nread == 6
                     && b[0]==0 && b[1]==1 && b[2]==2
                     && b[3]==3 && b[4]==4 && b[5]==5);
        assert_hard (file.read (Memory (b, i), NULL) == IoResult::Eof);
        assert_hard (file.readFull (Memory (b, i), NULL) == IoResult::Eof);
    }
}

static void testOne (Size const page_size)
{
    {
        Byte arr[] = { 0, 1, 2, 3, 4, 5 };
        MemoryFile file (Memory::forObject (arr));
        for (unsigned i = 1; i < 16; ++i) {
            doTestOne (page_size, i /* max_pages */, &file);
            assert_hard (file.seekBeg (0));
        }
    }
    {
        NativeFile file;
        assert_hard (file.open ("test_file_1", 0 /* open_flags */, FileAccessMode::ReadOnly));
        for (unsigned i = 1; i < 16; ++i) {
            doTestOne (page_size, i /* max_pages */, &file);
            assert_hard (file.seekBeg (0));
        }
    }
}

int main (void)
{
    libMaryInit ();

    for (unsigned i = 1; i <= 64; ++i)
        testOne (i /* page_size */);
}

