/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__CMDLINE__H__
#define LIBMARY__CMDLINE__H__


#include <libmary/iterator.h>


namespace M {

typedef bool (*ParseCmdlineCallback) (const char *short_name,
                                      const char *long_name,
                                      const char *value,
                                      void       *opt_data,
                                      void       *callback_data);

typedef void (*CmdlineCallback) (char const *value);

class CmdlineOption
{
public:
    const char *short_name,
               *long_name;

    bool  with_value;
    void *opt_data;

    ParseCmdlineCallback opt_callback;
    CmdlineCallback callback;

    CmdlineOption ()
        : short_name   (NULL),
          long_name    (NULL),
          with_value   (false),
          opt_data     (NULL),
          opt_callback (NULL),
          callback     (NULL)
    {}
};

void parseCmdline (int                        *argc,
                   char                     ***argv,
                   Iterator<CmdlineOption&>   &opt_iter,
                   ParseCmdlineCallback        callback     = NULL,
                   void                       *callbackData = NULL);

}


#endif /* LIBMARY__CMDLINE__H__ */

