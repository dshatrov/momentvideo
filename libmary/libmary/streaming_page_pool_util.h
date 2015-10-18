namespace M {

// TODO make all methods static
class StreamingPagePoolUtils {
#if 0
    template <class ...Args>
    static void doLogPagesMsg (LogLevel     const loglevel,
                               Page       *page,
                               Size        msg_offs,
                               Size        msg_len,
                               Args const &...args)
    {
        if (msg_len == 0) {
            return;
        }
        assert (page);

        while (page->data_len < msg_offs) {
            page = page->getNextMsgPage ();
            assert (page);
            msg_offs -= page->data_len;
        }

        Count i = 1;
        for (;;) {
            log_locked__ (loglevel, "Page #", i);

            Size tolog = page->data_len - msg_offs;
            if (tolog > msg_len)
                tolog = msg_len;

            logHex_locked__ (loglevel, page->mem().region (msg_offs, tolog), args...);

            msg_len -= tolog;
            if (msg_len == 0)
                break;

            msg_offs = 0;

            page = page->getNextMsgPage ();
            assert (page);
            ++i;
        }
        logs->flush ();
    }


    // logLevelOn() check should be performed before calling logPages_locked(),
    // otherwise there'll be unnecessary page_list walkthrough.
    //
    template <class ...Args>
    static void logPages_locked (LogLevel     const loglevel,
                                 Page       * const page,
                                 Size         const first_page_offs,
                                 Args const &...args)
    {
        exc_push_scope ();
        doLogPages (loglevel, page, first_page_offs, args...);
        exc_pop_scope ();
    }

    template <class ...Args>
    static void logPagesMsg (LogLevel     const loglevel,
                             Page       * const page,
                             Size         const msg_offs,
                             Size         const msg_len,
                             Args const &...args)
    {
        exc_push_scope ();
        logLock ();
        doLogPagesMsg (loglevel, page, msg_offs, msg_len, args...);
        logUnlock ();
        exc_pop_scope ();
    }

    template <class ...Args>
    static void logPagesMsg_locked (LogLevel     const loglevel,
                                    Page       * const page,
                                    Size         const msg_offs,
                                    Size         const msg_len,
                                    Args const &...args)
    {
        exc_push_scope ();
        doLogPagesMsg (loglevel, page, msg_offs, msg_len, args...);
        exc_pop_scope ();
    }

    // logLevelOn() check should be performed before calling logPages_locked(),
    // otherwise there'll be unnecessary page_list walkthrough.
    //
    template <class ...Args>
    static void logPages_locked (LogLevel     const loglevel,
                                 Page       * const page,
                                 Size         const first_page_offs,
                                 Args const &...args)
    {
        exc_push_scope ();
        doLogPages (loglevel, page, first_page_offs, args...);
        exc_pop_scope ();
    }

    template <class ...Args>
    static void logPagesMsg (LogLevel     const loglevel,
                             Page       * const page,
                             Size         const msg_offs,
                             Size         const msg_len,
                             Args const &...args)
    {
        exc_push_scope ();
        logLock ();
        doLogPagesMsg (loglevel, page, msg_offs, msg_len, args...);
        logUnlock ();
        exc_pop_scope ();
    }

    template <class ...Args>
    static void logPagesMsg_locked (LogLevel     const loglevel,
                                    Page       * const page,
                                    Size         const msg_offs,
                                    Size         const msg_len,
                                    Args const &...args)
    {
        exc_push_scope ();
        doLogPagesMsg (loglevel, page, msg_offs, msg_len, args...);
        exc_pop_scope ();
    }
#endif

    /*
    // TODO msg_len parameter
    static Count countPageListIovs (Page *first_page,
                                    Size  msg_offset);
    */

    /*
    // TODO msg_len parameter
    static void fillPageListIovs (Page     *first_page,
                                  Size      msg_offs,
                                  IovArray *iovs)
    */

    /*
    static bool msgEqual (Page *left_page,
                          Page *right_page);
    */

    static bool pagesEqualToMemory (StreamingPage *page,
                                    Size           msg_offs,
                                    Size           msg_len,
                                    ConstMemory    mem);

    static void copyMsgToMemory (StreamingPage *page,
                                 Size           msg_offs,
                                 Size           msg_len,
                                 Byte          *buf);

    /*
    static Uint32 calculateChecksumPages (StreamingPage *first_page,
                                          Size           msg_offs);
    */

    static Uint64 calculateChecksumPages (StreamingPage *first_page,
                                          Size           msg_offs,
                                          Size           msg_len);
};

}

