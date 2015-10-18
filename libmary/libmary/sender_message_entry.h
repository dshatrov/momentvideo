/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__SENDER_MESSAGE_ENTRY__H__
#define LIBMARY__SENDER_MESSAGE_ENTRY__H__


#include <libmary/types.h>
#include <libmary/intrusive_list.h>
#include <libmary/page_pool.h>


// M::VSlab is a grow-only data structure. Be warned.
#define LIBMARY_SENDER_VSLAB


#ifdef LIBMARY_SENDER_VSLAB
  #include <libmary/vslab.h>
#endif


namespace M {

class SenderMessageEntry_MessageList_name;
#ifdef LIBMARY_WIN32_IOCP
  class SenderMessageEntry_PendingMessageList_name;
#endif

class SenderMessageEntry : public IntrusiveListElement<SenderMessageEntry_MessageList_name>
                       #ifdef LIBMARY_WIN32_IOCP
                         , public IntrusiveListElement<SenderMessageEntry_PendingMessageList_name>
                       #endif
{
  public:
    virtual Size getTotalMsgLen() const = 0;

    virtual bool isEmpty () = 0;

    virtual bool getSeek (Uint64 * const ret_offset)
    {
        if (ret_offset)
            *ret_offset = 0;
        
        return false;
    }

    virtual void seekDone () {}

    virtual void fillIovs (IovArray *iovs,
                           Size      max_iovs) = 0;

    // Returns 'true' if the message has been sent in full.
    virtual bool react (Size  bytes_sent,
                        Size * mt_nonnull ret_accepted) = 0;

    virtual void release () = 0;

    virtual ~SenderMessageEntry () {}
};

typedef IntrusiveList <SenderMessageEntry, SenderMessageEntry_MessageList_name> SenderMessageEntry_MessageList;
#ifdef LIBMARY_WIN32_IOCP
  typedef IntrusiveList <SenderMessageEntry, SenderMessageEntry_PendingMessageList_name> SenderMessageEntry_PendingMessageList;
#endif

class SenderMessageEntry_Common : public SenderMessageEntry
{
  public:
    Size header_len;

    PagePool *page_pool;
    PagePool::Page *first_page;
    Size msg_offs;
    Size msg_len;
};

// This MessageEntry is different from Sender::MessageEntry in that it doesn't use
// msgUnref() to release pages, which allows to send parts of long page lists.
class SenderMessageEntry_Pages : public SenderMessageEntry_Common
{
  #ifdef LIBMARY_SENDER_VSLAB
    friend class VSlab<SenderMessageEntry_Pages>;
  #endif

  private:
    PagePool::Page *cur_page;
    Size header_offs;
    Size page_offs;
    Size msg_left;

    #ifdef LIBMARY_SENDER_VSLAB
      VSlab<SenderMessageEntry_Pages>::AllocKey vslab_key;
    #endif

  public:
    Byte* getHeaderData () { return (Byte*) this + sizeof (*this); }

    mt_iface (SenderMessageEntry)
      Size getTotalMsgLen () const;

      bool isEmpty ();

      void fillIovs (IovArray *iovs,
                     Size      max_iovs);

      bool react (Size  bytes_sent,
                  Size * mt_nonnull ret_accepted);

      void release ();
    mt_iface_end

    // Initializes all except header_len.
    void init (PagePool::Page * const first_page,
               PagePool       * const page_pool,
               Size             const msg_offs,
               Size             const msg_len)
    {
        this->header_len = 0;

        this->first_page = first_page;
        this->page_pool  = page_pool;
        this->msg_offs   = msg_offs;
        this->msg_len    = msg_len;

        this->cur_page    = first_page;
        this->header_offs = 0;
        this->page_offs   = msg_offs;
        this->msg_left    = msg_len;
    }

    static SenderMessageEntry_Pages* createNew (Size header_len);
};

class SenderMessageEntry_Seek : public SenderMessageEntry
{
  private:
    Uint64 const offset;
    bool done;

  public:
    mt_iface (SenderMessageEntry)
      Size getTotalMsgLen() const { return 0; }

      bool isEmpty () { return true; }

      bool getSeek (Uint64 * const ret_offset)
      {
          if (done) {
              if (ret_offset)
                  *ret_offset = 0;

              return false;
          }

          if (ret_offset)
              *ret_offset = offset;
          
          return true;
      }

      void seekDone () { done = true; }

      void fillIovs (IovArray * const /* iovs */,
                     Size       const /* max_iovs */)
      {}

      // Returns 'true' if the message has been sent in full.
      bool react (Size   const /* bytes_sent */,
                  Size * const mt_nonnull ret_accepted )
      {
          *ret_accepted = 0;
          return done;
      }

      void release () { delete this; }
    mt_iface_end

    SenderMessageEntry_Seek (Uint64 const offset)
        : offset (offset),
          done   (false)
    {}
};

}


#endif /* LIBMARY__SENDER_MESSAGE_ENTRY__H__ */

