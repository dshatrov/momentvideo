/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>
#include <libmary/util_dev.h>

#include <libmary/line_server.h>


namespace M {

Receiver::Frontend const LineServer::receiver_frontend = {
    processInput,
    processEof,
    processError,
    NULL /* processInputFrom */
};

Receiver::ProcessInputResult
LineServer::processInput (Memory   const _mem,
                          Size   * const mt_nonnull ret_accepted,
                          void   * const  _self)
{
    LineServer * const self = static_cast <LineServer*> (_self);
    Memory mem = _mem;

    *ret_accepted = 0;

    for (;;) {
        assert (mem.len() >= self->recv_pos);
        if (mem.len() == self->recv_pos) {
          // No new data since the last input event => nothing changed.
            return Receiver::ProcessInputResult::Again;
        }

        Size cr_pos;
        {
            Byte const *cr_ptr = NULL;
            for (Size i = self->recv_pos, i_end = mem.len(); i < i_end; ++i) {
                if (mem.mem() [i] == 13 /* CR */ ||
                    mem.mem() [i] == 10 /* LF */)
                {
                    cr_ptr = mem.mem() + i;
                    break;
                }
            }

            if (!cr_ptr) {
                self->recv_pos = mem.len();
                if (self->recv_pos >= self->max_line_len)
                    return Receiver::ProcessInputResult::Error;

                return Receiver::ProcessInputResult::Again;
            }

            cr_pos = cr_ptr - mem.mem();
        }

        Size line_start = 0;
        while (mem.mem() [line_start] == 13 /* CR */ ||
               mem.mem() [line_start] == 10 /* LF */)
        {
            ++line_start;
        }

        if (line_start >= cr_pos) {
            self->recv_pos = 0;
            mem = mem.region (line_start);
            *ret_accepted += line_start;
            continue;
        }

        Size const next_pos = cr_pos + 1;

        if (self->frontend)
            self->frontend.call (self->frontend->line, mem.region (line_start, cr_pos));

        self->recv_pos = 0;
        mem = mem.region (next_pos);
        *ret_accepted += next_pos;
    }

    return Receiver::ProcessInputResult::Normal;
}

void
LineServer::processEof (Memory   const /* unprocessed_mem */,
                        void   * const _self)
{
    LineServer * const self = static_cast <LineServer*> (_self);
    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

void
LineServer::processError (Exception * const /* exc_ */,
                          Memory      const /* unprocessed_mem */,
                          void      * const _self)
{
    LineServer * const self = static_cast <LineServer*> (_self);
    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

void
LineServer::init (Receiver         * const  receiver,
                  CbDesc<Frontend>   const &frontend,
                  Size               const  max_line_len)
{
    this->frontend = frontend;
    this->max_line_len = max_line_len;

    receiver->setFrontend (CbDesc<Receiver::Frontend> (&receiver_frontend, this, this));
}

LineServer::LineServer (EmbedContainer * const embed_container)
    : Object (embed_container),
      max_line_len (4096),
      recv_pos (0)
{
}

LineServer::~LineServer ()
{
}

ConstMemory lineEscape (ConstMemory     const line,
                        StRef<String> * const mt_nonnull ret_ref)
{
    Byte const * const buf = line.buf();
    Size const len = line.len();
    Count esc_len = len;
    bool esc = false;
    for (Size i = 0; i < len; ++i) {
        if (buf [i] == ' ') {
            esc = true;
        } else
        if (buf [i] == '_' || buf [i] == '\\') {
            ++esc_len;
            esc = true;
        }
    }

    if (esc) {
        *ret_ref = st_grab (new (std::nothrow) String (esc_len));

        Memory const dst_mem = (*ret_ref)->mem();
        Byte * const dst_buf = dst_mem.buf();
        Size pos = 0;
        for (Size i = 0; i < len; ++i) {
            if (buf [i] == ' ') {
                dst_buf [pos] = '_';
            } else
            if (buf [i] == '_') {
                dst_buf [pos] = '\\';
                ++pos;
                dst_buf [pos] = '_';
            } else
            if (buf [i] == '\\') {
                dst_buf [pos] = '\\';
                ++pos;
                dst_buf [pos] = '\\';
            }

            ++pos;
        }

        return dst_mem;
    }

    return line;
}

}

