/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__PAGE_REQUEST_MANAGER__H__
#define MOMENT__PAGE_REQUEST_MANAGER__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class PageRequestManager : public Object
{
  private:
    StateMutex mutex;

  public:
    struct PageRequestResult {
        enum Value {
            Success,
            NotFound,
            AccessDenied,
            InternalError
        };
        operator Value () const { return value; }
        PageRequestResult (Value const value) : value (value) {}
        PageRequestResult () {}
      private:
        Value value;
    };

    struct PageRequestSection : public StReferenced {};

    struct PageRequest {
        // If ret.mem() == NULL, then the parameter is not set.
        // If ret.len() == 0, then the parameter has empty value.
        virtual ConstMemory               getParameter     (ConstMemory name) = 0;
        virtual IpAddress                 getClientAddress () = 0;
        virtual void                      addHashVar       (PageRequestSection *section, ConstMemory name, ConstMemory value) = 0;
        virtual StRef<PageRequestSection> addSection       (PageRequestSection *section, ConstMemory name) = 0;
        virtual void                      showSection      (PageRequestSection *section, ConstMemory name) = 0;
        virtual ~PageRequest () {}
    };

    struct PageRequestHandler {
        PageRequestResult (*pageRequest) (PageRequest  *req,
                                          ConstMemory   path,
                                          ConstMemory   full_path,
                                          void         *cb_data);
    };

  private:
    class PageRequestHandlerEntry;
    typedef StringHash< Ref<PageRequestHandlerEntry> > PageRequestHandlerHash;

    class PageRequestHandlerEntry : public Object
    {
    public:
        StateMutex mutex;

        Informer_<PageRequestHandler> event_informer;

        mt_const PageRequestHandlerHash::EntryKey hash_key;

        mt_mutex (PageRequestManager::mutex) Count num_handlers;

        static void informPageRequest (PageRequestHandler *handler,
                                       void               *cb_data,
                                       void               *inform_data);

        PageRequestResult firePageRequest (PageRequest *page_req,
                                           ConstMemory  path,
                                           ConstMemory  full_path);

        PageRequestHandlerEntry (EmbedContainer * const embed_container)
            : Object         (embed_container),
              event_informer (this, &mutex),
              num_handlers   (0)
        {}
    };

    mt_mutex (mutex) PageRequestHandlerHash page_handler_hash;

  public:
    class PageRequestHandlerKey
    {
        friend class PageRequestManager;
      private:
        PageRequestHandlerEntry          *handler_entry;
        GenericInformer::SubscriptionKey  sbn_key;
    };

    PageRequestHandlerKey addPageRequestHandler (CbDesc<PageRequestHandler> const &cb,
                                                 ConstMemory path);

    void removePageRequestHandler (PageRequestHandlerKey handler_key);

    PageRequestResult processPageRequest (PageRequest *page_req,
                                          ConstMemory  path);

    PageRequestManager (EmbedContainer *embed_container);
};

}


#endif /* MOMENT__PAGE_REQUEST_MANAGER__H__ */

