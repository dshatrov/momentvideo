/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-rtmp/util_rtmp.h>


namespace Moment {

namespace RtmpUtil {

// TODO Figure out the actual form of UnpublishNotify - untested
Result sendNetStreamPlayUnpublishNotify (RtmpConnection * const mt_nonnull rtmp_conn,
                                         Uint32           const msg_stream_id,
                                         ConstMemory      const stream_name)
{
    AmfAtom atoms [13];
    AmfEncoder encoder (atoms);

    encoder.addString ("onStatus");
    encoder.addNumber (0.0 /* transaction_id */);
    encoder.addNullObject ();

    encoder.beginObject ();

    encoder.addFieldName ("level");
    encoder.addString ("status");

    encoder.addFieldName ("code");
    encoder.addString ("NetStream.Play.UnpublishNotify");

    encoder.addFieldName ("description");
    StRef<String> const desc_str = makeString (stream_name, " is now unpublished");
    encoder.addString (desc_str->mem());

    encoder.addFieldName ("clientid");
    encoder.addNumber (1.0);

    encoder.endObject ();

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_func, "could not encode onStatus message");
        return Result::Failure;
    }

    rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    return Result::Success;
}

Result sendNetStreamPlayPublishNotify (RtmpConnection * const mt_nonnull rtmp_conn,
                                       Uint32           const msg_stream_id,
                                       ConstMemory      const stream_name)
{
    AmfAtom atoms [13];
    AmfEncoder encoder (atoms);

    encoder.addString ("onStatus");
    encoder.addNumber (0.0 /* transaction_id */);
    encoder.addNullObject ();

    encoder.beginObject ();

    encoder.addFieldName ("level");
    encoder.addString ("status");

    encoder.addFieldName ("code");
    encoder.addString ("NetStream.Play.PublishNotify");

    encoder.addFieldName ("description");
    StRef<String> const desc_str = makeString (stream_name, " is now published");
    encoder.addString (desc_str->mem());

    encoder.addFieldName ("clientid");
    encoder.addNumber (1.0);

    encoder.endObject ();

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_func, "could not encode onStatus message");
        return Result::Failure;
    }

    rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    return Result::Success;
}

Result sendNetStreamPlayStreamNotFound (RtmpConnection * const mt_nonnull rtmp_conn,
                                        Uint32           const msg_stream_id,
                                        ConstMemory      const stream_name)
{
    AmfAtom atoms [15];
    AmfEncoder encoder (atoms);

    encoder.addString ("onStatus");
    encoder.addNumber (0.0 /* transaction_id */);
    encoder.addNullObject ();

    encoder.beginObject ();

    encoder.addFieldName ("level");
    encoder.addString ("error");

    encoder.addFieldName ("code");
    encoder.addString ("NetStream.Play.StreamNotFound");

    encoder.addFieldName ("description");
    StRef<String> const desc_str = makeString ("Failed to play ", stream_name, "; stream not found.");
    encoder.addString (desc_str->mem());

    encoder.addFieldName ("clientid");
    encoder.addNumber (1.0);

    encoder.addFieldName ("details");
    encoder.addString (stream_name);

    encoder.endObject ();

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_func, "could not encode onStatus message");
        return Result::Failure;
    }

    rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    return Result::Success;
}

Result sendNetStreamPlayStart (RtmpConnection * const mt_nonnull rtmp_conn,
                               Uint32           const msg_stream_id,
                               ConstMemory      const stream_name)
{
  // Sending onStatus reply "Start".

    AmfAtom atoms [17];
    AmfEncoder encoder (atoms);

    encoder.addString ("onStatus");
    encoder.addNumber (0.0 /* transaction_id */);
    encoder.addNullObject ();

    encoder.beginObject ();

    encoder.addFieldName ("level");
    encoder.addString ("status");

    encoder.addFieldName ("code");
    encoder.addString ("NetStream.Play.Start");

    encoder.addFieldName ("description");
    StRef<String> const desc_str = makeString ("Started playing ", stream_name, ".");
    encoder.addString (desc_str->mem());

    encoder.addFieldName ("details");
    encoder.addString (stream_name);

    encoder.addFieldName ("clientid");
    encoder.addNumber (1.0);

    encoder.addFieldName ("isFastPlay");
    encoder.addBoolean (false);

    encoder.endObject ();

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_func, "could not encode onStatus message");
        return Result::Failure;
    }

    rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    return Result::Success;
}

Result sendNetStreamDataStart (RtmpConnection * const mt_nonnull rtmp_conn,
                               Uint32           const msg_stream_id)
{
    AmfAtom atoms [9];
    AmfEncoder encoder (atoms);

    encoder.addString ("onStatus");
    encoder.addNumber (0.0 /* transaction_id */);
    encoder.addNullObject ();

    encoder.beginObject ();

    encoder.addFieldName ("level");
    encoder.addString ("status");

    encoder.addFieldName ("code");
    encoder.addString ("NetStream.Data.Start");

    encoder.endObject ();

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_func, "could not encode onStatus message");
        return Result::Failure;
    }

    rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    return Result::Success;
}

Result sendNetStreamPlayReset (RtmpConnection * const mt_nonnull rtmp_conn,
                               Uint32           const msg_stream_id,
                               ConstMemory      const stream_name)
{
  // Sending onStatus reply "Reset".

    AmfAtom atoms [15];
    AmfEncoder encoder (atoms);

    encoder.addString ("onStatus");
    encoder.addNumber (0.0 /* transaction_id */);
    encoder.addNullObject ();

    encoder.beginObject ();

    encoder.addFieldName ("level");
    encoder.addString ("status");

    encoder.addFieldName ("code");
    encoder.addString ("NetStream.Play.Reset");

    encoder.addFieldName ("description");
    StRef<String> const desc_str = makeString ("Playing and resetting ", stream_name, ".");
    encoder.addString (desc_str->mem());

    encoder.addFieldName ("details");
    encoder.addString (stream_name);

    encoder.addFieldName ("clientid");
    encoder.addNumber (1.0);

    encoder.endObject ();

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_func, "could not encode onStatus message");
        return Result::Failure;
    }

    rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    return Result::Success;
}

Result sendRtmpSampleAccess (RtmpConnection * const mt_nonnull rtmp_conn,
                             Uint32           const msg_stream_id,
                             bool             const allow_a,
                             bool             const allow_b)
{
  // Sending |RtmpSampleAccess

    AmfAtom atoms [3];
    AmfEncoder encoder (atoms);

    encoder.addString ("|RtmpSampleAccess");
    encoder.addBoolean (allow_a);
    encoder.addBoolean (allow_b);

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_func, "could not encode |RtmpSampleAccess message");
        return Result::Failure;
    }

    rtmp_conn->sendDataMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    return Result::Success;
}

Result sendNetStreamPublishStart (RtmpConnection * const mt_nonnull rtmp_conn,
                                  Uint32           const msg_stream_id,
                                  ConstMemory      const stream_name)
{
  // Sending onStatus reply "Start".

    AmfAtom atoms [13];
    AmfEncoder encoder (atoms);

    encoder.addString ("onStatus");
    encoder.addNumber (0.0 /* transaction_id */);
    encoder.addNullObject ();

    encoder.beginObject ();

    encoder.addFieldName ("level");
    encoder.addString ("status");

    encoder.addFieldName ("code");
    encoder.addString ("NetStream.Publish.Start");

    encoder.addFieldName ("description");
    StRef<String> const desc_str = makeString ("Publishing ", stream_name, ".");
    encoder.addString (desc_str->mem());

    encoder.addFieldName ("clientid");
    encoder.addNumber (1.0);

    encoder.endObject ();

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_func, "could not encode onStatus message");
        return Result::Failure;
    }

    rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    return Result::Success;
}

Result sendMetaData (RtmpConnection  * const mt_nonnull rtmp_conn,
                     Uint32            const msg_stream_id,
                     MediaDesc_Audio * const audio_desc,
                     MediaDesc_Video * const video_desc)
{
    PagePool::PageListHead page_list;

    AmfAtom atoms [128];
    AmfEncoder encoder (atoms);

    encoder.addString ("onMetaData");

    encoder.beginEcmaArray (0 /* num_entries */);
    // Unused
    // AmfAtom * const toplevel_array_atom = encoder.getLastAtom ();
    Uint32 num_entries = 0;

    /*
    encoder.addFieldName ("author");
    encoder.addString ("");
    ++num_entries;

    encoder.addFieldName ("copyright");
    encoder.addString ("");
    ++num_entries;

    encoder.addFieldName ("description");
    encoder.addString ("");
    ++num_entries;

    encoder.addFieldName ("keywords");
    encoder.addString ("");
    ++num_entries;

    encoder.addFieldName ("rating");
    encoder.addString ("");
    ++num_entries;

    encoder.addFieldName ("title");
    encoder.addString ("");
    ++num_entries;
    */

    /*
    encoder.addFieldName ("presetname");
    encoder.addString ("Custom");
    ++num_entries;
    */

    /*
    encoder.addFieldName ("creationdate");
    encoder.addString ("Mon Oct 21 03:25:40 2013");
    ++num_entries;
    */

    if (video_desc) {
        /*
        encoder.addFieldName ("videodevice");
        encoder.addString ("Camera");
        ++num_entries;
        */

        /*
        encoder.addFieldName ("framerate");
        encoder.addNumber (25.0);
        ++num_entries;
        */

        encoder.addFieldName ("width");
        encoder.addNumber (video_desc->width);
        ++num_entries;

        encoder.addFieldName ("height");
        encoder.addNumber (video_desc->height);
        ++num_entries;

        encoder.addFieldName ("frameWidth");
        encoder.addNumber (video_desc->width);
        ++num_entries;

        encoder.addFieldName ("frameHeight");
        encoder.addNumber (video_desc->height);
        ++num_entries;

        encoder.addFieldName ("displayWidth");
        encoder.addNumber (video_desc->width);
        ++num_entries;

        encoder.addFieldName ("displayHeight");
        encoder.addNumber (video_desc->height);
        ++num_entries;

        if (video_desc->getType() == MediaDesc_Video::AVC) {
            MediaDesc_AVC * const avc_desc = static_cast <MediaDesc_AVC*> (video_desc);

            encoder.addFieldName ("videocodecid");
            encoder.addString ("avc1");
            ++num_entries;

            /*
            encoder.addFieldName ("videodatarate");
            encoder.addNumber (500.0);
            ++num_entries;
            */

            if (avc_desc->getCodecData().len() >= 4) {
                encoder.addFieldName ("avclevel");
                encoder.addNumber (avc_desc->getCodecData().buf() [3]);
                ++num_entries;

                encoder.addFieldName ("avcprofile");
                encoder.addNumber (avc_desc->getCodecData().buf() [1]);
                ++num_entries;
            }
        }

        /*
        encoder.addFieldName ("videokeyframe_frequency");
        encoder.addNumber (5.0);
        ++num_entries;
        */
    }

    if (audio_desc) {
        /*
        encoder.addFieldName ("audiodevice");
        encoder.addString ("Microphone");
        ++num_entries;
        */

        encoder.addFieldName ("audiosamplerate");
        encoder.addNumber (audio_desc->rate);
        ++num_entries;

        encoder.addFieldName ("audiochannels");
        encoder.addNumber (audio_desc->channels);
        ++num_entries;

        /*
        encoder.addFieldName ("audioinputvolume");
        encoder.addNumber (75.0);
        ++num_entries;
        */

        /*
        encoder.addFieldName ("audiodatarate");
        encoder.addNumber (64.0);
        ++num_entries;
        */
    }

    encoder.endObject ();

    // Unnecessary and wrong
    // toplevel_array_atom->setEcmaArraySize (num_entries);

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_func, "could not encode onMetaData message");
        return Result::Failure;
    }

    rtmp_conn->sendDataMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    return Result::Success;
}

}

}

