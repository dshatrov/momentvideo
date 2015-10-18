/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ANNOTATIONS__H__
#define LIBMARY__ANNOTATIONS__H__


// It is preferred not to include this header directly.
// Include types.h instead.

// Note that this source file may be included in plain C source files.


#define mt_begin
#define mt_end

// Function parameters marked with 'mt_nonnull' cannot be null. If null is
// passed for such a parameter, then function's behaviour is undefined.
#define mt_nonnull

// TODO Abolish mt_throw in favor of mt_throws
#define mt_throw(a)
// Throws LibMary exceptions.
#define mt_throws
// Throws C++ exceptions.
#define mt_throws_cpp

// Functions marked with 'mt_locked' lock state mutexes of their objects when
// called. They are called "locked functions".
#define mt_locked

#define mt_const

// The value is stored just once, but not before concurrent access.
// It's safe to load the value without synchronization if it's guaranteed
// loads happen after store.
#define mt_once

#define mt_mutex(a)
#define mt_locks(a)
#define mt_unlocks(a)
#define mt_unlocks_locks(a)
#define mt_sync(a)

#define mt_iface(a)
#define mt_iface_end

// Functions marked with mt_async may call arbitrary callbacks.
#define mt_async
#define mt_unsafe

#define mt_one_of(a)
#define mt_sync_domain(a)


#endif /* LIBMARY__ANNOTATIONS__H__ */

