/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__SERVER_CONTEXT__H__
#define LIBMARY__SERVER_CONTEXT__H__


#include <libmary/timers.h>
#include <libmary/active_poll_group.h>
#include <libmary/deferred_processor.h>
#include <libmary/deferred_connection_sender.h>


namespace M {

class ServerThreadContext : public Object
{
  private:
    mt_const Ref<Timers>                        timers;
    mt_const Ref<PollGroup>                     poll_group;
    mt_const Ref<DeferredProcessor>             deferred_processor;
    mt_const Ref<DeferredConnectionSenderQueue> dcs_queue;

    DeletionContext deletion_ctx;

  public:
    Timers*                        getTimers                        () const { return timers; }
    PollGroup*                     getPollGroup                     () const { return poll_group; }
    DeferredProcessor*             getDeferredProcessor             () const { return deferred_processor; }
    DeferredConnectionSenderQueue* getDeferredConnectionSenderQueue () const { return dcs_queue; }

    DeletionContext* getDeletionContext () { return &deletion_ctx; }

    mt_const void init (Timers                        * const timers,
                        ActivePollGroup               * const poll_group,
                        DeferredProcessor             * const deferred_processor,
                        DeferredConnectionSenderQueue * const dcs_queue)
    {
        this->timers             = timers;
        this->poll_group         = poll_group;
        this->deferred_processor = deferred_processor;
        this->dcs_queue          = dcs_queue;

        deletion_ctx.init (poll_group);
    }

    ServerThreadContext (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

class ServerContext : public Object
{
  public:
    virtual Ref<ServerThreadContext> selectThreadContext  () = 0;
    virtual Ref<ServerThreadContext> getMainThreadContext () = 0;

    ServerContext (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}

    virtual ~ServerContext () {}
};

}


#endif /* LIBMARY__SERVER_CONTEXT__H__ */

