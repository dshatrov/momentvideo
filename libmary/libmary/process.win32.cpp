/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/process.h>


namespace M {

Result spawnProcess (ConstMemory          const path,
                     ConstMemory        * const args,
                     Count                const num_args,
                     Count                const num_input_pipes,
                     ProcessInputPipe  ** const ret_input_pipes,
                     Count                const num_output_pipes,
                     ProcessOutputPipe ** const ret_output_pipes)
{
    return Result::Failure;
}

Result initProcessSpawner ()
{
    return Result::Success;
}

Result initProcessSpawner_addExitPoll (PollGroup * const mt_nonnull poll_group)
{
    return Result::Success;
}

}

