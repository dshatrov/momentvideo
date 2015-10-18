/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/page_request_manager.h>


namespace Moment {

namespace {
    class InformPageRequest_Data {
      public:
        PageRequestManager::PageRequest * const page_req;
        ConstMemory const path;
        ConstMemory const full_path;

        PageRequestManager::PageRequestResult result;

        InformPageRequest_Data (PageRequestManager::PageRequest * const page_req,
                                ConstMemory const path,
                                ConstMemory const full_path)
            : page_req  (page_req),
              path      (path),
              full_path (full_path),
              result    (PageRequestManager::PageRequestResult::Success)
        {}
    };
}

void
PageRequestManager::PageRequestHandlerEntry::informPageRequest (PageRequestHandler * const handler,
                                                                void               * const cb_data,
                                                                void               * const _inform_data)
{
    InformPageRequest_Data * const inform_data = static_cast <InformPageRequest_Data*> (_inform_data);

    assert (handler->pageRequest);
    PageRequestResult const res = handler->pageRequest (inform_data->page_req,
                                                        inform_data->path,
                                                        inform_data->full_path,
                                                        cb_data);
    if (inform_data->result == PageRequestResult::Success)
        inform_data->result = res;
}

PageRequestManager::PageRequestResult
PageRequestManager::PageRequestHandlerEntry::firePageRequest (PageRequest * const page_req,
                                                              ConstMemory   const path,
                                                              ConstMemory   const full_path)
{
    InformPageRequest_Data inform_data (page_req, path, full_path);
    event_informer.informAll (informPageRequest, &inform_data);
    return inform_data.result;
}

PageRequestManager::PageRequestHandlerKey
PageRequestManager::addPageRequestHandler (CbDesc<PageRequestHandler> const &cb,
                                           ConstMemory const path)
{
    PageRequestHandlerEntry          *handler_entry;
    GenericInformer::SubscriptionKey  sbn_key;

    mutex.lock ();
    {
        PageRequestHandlerHash::EntryKey const hash_key = page_handler_hash.lookup (path);
        if (hash_key) {
            handler_entry = hash_key.getData();
        } else {
            handler_entry = new (std::nothrow) PageRequestHandlerEntry (NULL /* embed_container */);
            handler_entry->hash_key = page_handler_hash.add (path, handler_entry);
        }

        ++handler_entry->num_handlers;
    }
    mutex.unlock ();

    sbn_key = handler_entry->event_informer.subscribe (cb);

    PageRequestHandlerKey handler_key;
    handler_key.handler_entry = handler_entry;
    handler_key.sbn_key       = sbn_key;

    return handler_key;
}

void
PageRequestManager::removePageRequestHandler (PageRequestHandlerKey const handler_key)
{
    PageRequestHandlerEntry * const handler_entry = handler_key.handler_entry;

    handler_entry->event_informer.unsubscribe (handler_key.sbn_key);

    mutex.lock ();
    --handler_entry->num_handlers;
    if (handler_entry->num_handlers == 0) {
        page_handler_hash.remove (handler_entry->hash_key);
    }
    mutex.unlock ();
}

PageRequestManager::PageRequestResult
PageRequestManager::processPageRequest (PageRequest * const page_req,
                                        ConstMemory   const path)
{
    logD_ (_func, "path: ", path);

    mutex.lock ();

    PageRequestHandlerHash::EntryKey const hash_key = page_handler_hash.lookup (path);
    if (!hash_key) {
        mutex.unlock ();
        logD_ (_func, "no handlers, path: ", path);
        return PageRequestResult::Success;
    }

    Ref<PageRequestHandlerEntry> const handler = hash_key.getData();

    mutex.unlock ();

    PageRequestResult const res = handler->firePageRequest (page_req,
                                                            path,
                                                            path /* full_path */);
    return res;
}

PageRequestManager::PageRequestManager (EmbedContainer * const embed_container)
    : Object (embed_container)
{
}

}

