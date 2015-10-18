/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__RECEIVER_H__
#define LIBMARY__RECEIVER_H__


#include <libmary/types.h>
#include <libmary/cb.h>
#include <libmary/exception.h>
#include <libmary/util_net.h>


namespace M {

class Receiver : public Object
{
  public:
    enum_beg (ProcessInputResult)
        enum_values (
	    // TODO "Normal" is equal to "Again" + ret_accepted = mem.len().
	    //      Rename "Again" to "Normal.
	    Normal,
	    Error,
	    // The buffer does not contain a complete message. Buffer contents
	    // must be shifted to the beginning of the buffer and more data
	    // should be received before processing.
	    Again,
	    // The buffer contains data which has not been analyzed yet.
	    // This return code does not require buffer contents to be moved
	    // to the beginning of the buffer.
            InputBlocked
        )
    enum_end (ProcessInputResult)

    struct Frontend {
	ProcessInputResult (*processInput) (Memory  mem,
					    Size   * mt_nonnull ret_accepted,
					    void   *cb_data);

	void (*processEof) (Memory  unprocessed_mem,
                            void   *cb_data);

	void (*processError) (Exception *exc_,
                              Memory     unprocessed_mem,
			      void      *cb_data);

        ProcessInputResult (*processInputFrom) (Memory     mem,
                                                IpAddress  addr,
                                                Size      * mt_nonnull ret_accepted,
                                                void      *cb_data);
    };

  protected:
    mt_const Cb<Frontend> frontend;

  public:
    virtual void unblockInput () = 0;

    mt_const void setFrontend (CbDesc<Frontend> const &frontend)
        { this->frontend = frontend; }

    Receiver (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

}


#endif /* LIBMARY__RECEIVER_H__ */

