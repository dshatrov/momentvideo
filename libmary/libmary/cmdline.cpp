/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <libmary/io.h>

#include <libmary/cmdline.h>


#define DEBUG(a) ;


namespace M {

void
parseCmdline (int                        *argc,
	      char                     ***argv,
	      Iterator<CmdlineOption&>   &opt_iter,
	      ParseCmdlineCallback        callback,
	      void                       *callbackData)
{
    if (argc == NULL ||
	argv == NULL)
    {
	return;
    }

    if (*argc <= 1)
	return;

    unsigned long nopts_consumed = 0;
    unsigned long cur_opt = 1,
		  cur_opt_shifted = 1;
    while (cur_opt < (unsigned long) *argc) {
	DEBUG (
          errs->println ("MyCpp.parseCmdline: "
                         "cur_opt: ",
                         cur_opt,
                         ", cur_opt_shifted: ",
                         cur_opt_shifted);

          errs->println ("MyCpp.parseCmdline: option: \"",
                         (*argv) [cur_opt_shifted],
                         "\"");
	)

	bool long_option;
	if ((*argv) [cur_opt_shifted] [0] == '-') {
	    if ((*argv) [cur_opt_shifted] [1] == '-') {
		DEBUG (
                  errs->println ("MyCpp.parseCmdline: long option");
		)

		long_option = true;
	    } else {
		DEBUG (
                  errs->println ("MyCpp.parseCmdline: short option");
		)

		long_option = false;
	    }
	} else {
	    DEBUG (
              errs->println ("MyCpp.parseCmdline: not an option");
	    )

	    cur_opt ++;
	    cur_opt_shifted ++;
	    continue;
	}

	unsigned long argv_shift = 0;

	opt_iter.reset ();
	while (!opt_iter.done ()) {
	    CmdlineOption &opt = opt_iter.next ();

	    bool match = false;
	    if (long_option) {
		if (opt.long_name == NULL)
		    continue;

                char const * const name = (*argv) [cur_opt_shifted] + 2;
		if (equal (ConstMemory (name, strlen (name)),
                           ConstMemory (opt.long_name, strlen (opt.long_name))))
                {
		    DEBUG (
                      errs->println ("MyCpp.parseCmdline: long option match");
		    )

		    match = true;
		}
	    } else {
		if (opt.short_name == NULL)
		    continue;

                char const * const name = (*argv) [cur_opt_shifted] + 1;
		if (equal (ConstMemory (name, strlen (name)),
                           ConstMemory (opt.short_name, strlen (opt.short_name))))
                {
		    match = true;
                }
	    }

	    if (!match)
		continue;

	    nopts_consumed ++;

	    argv_shift = 1;

	    const char *value = NULL;
	    if (opt.with_value &&
		cur_opt < (unsigned long) *argc - 1)
	    {
		value = (*argv) [cur_opt_shifted + 1];
		nopts_consumed ++;
		argv_shift ++;
	    }

	    unsigned long i,
			  j = 0;
	    for (i = cur_opt_shifted;
		 i < (unsigned long) *argc - argv_shift;
		 i++)
	    {
		DEBUG (
                  errs->println ("MyCpp.parseCmdline: shifting "
                                 "argv [",
                                 cur_opt_shifted + j + argv_shift,
                                 "] to argv [",
                                 cur_opt_shifted + j,
                                 "]");
		)

		(*argv) [cur_opt_shifted + j] =
			(*argv) [cur_opt_shifted + j + argv_shift];
		j ++;
	    }

	    if (opt.opt_callback) {
                if (!opt.opt_callback (opt.short_name,
                                       opt.long_name,
                                       value,
                                       opt.opt_data,
                                       callbackData))
                {
		    goto _stop_parsing;
                }
	    }

            if (opt.callback)
                opt.callback (value);

	    if (callback) {
                if (!callback (opt.short_name,
                               opt.long_name,
                               value,
                               opt.opt_data,
                               callbackData))
                {
		    goto _stop_parsing;
                }
	    }

	    break;
	}

	if (argv_shift == 0) {
	    cur_opt ++;
	    cur_opt_shifted ++;
	} else
	    cur_opt += argv_shift;
    }

_stop_parsing:
    *argc -= nopts_consumed;
}

}

