/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/channel_options.h>


namespace Moment {

void
ChannelOptions::toConfigStringPages (PagePool               * mt_nonnull const page_pool,
                                     PagePool::PageListInfo * mt_nonnull const page_list)
{
    PagePool::PageListOutputStream out (page_pool, page_list);

    if (output_opts) {
        List< Ref<OutputDesc> >::iterator iter (output_opts->output_list);
        while (!iter.done()) {
            OutputDesc * const output_desc = iter.next()->data;

            out.print ("output {\n");
            output_desc->config_section->dump (&out);
            out.print ("}\n");
        }
    }

    out.print ("no_video_timeout = ", no_video_timeout, "\n");
    out.print ("force_no_video_timeout = ", force_no_video_timeout, "\n");
}

}

