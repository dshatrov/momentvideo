/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/mp4_reader.h>


#define MOMENT__STTS_ENTRY_SIZE   8
#define MOMENT__STTS_DATA_OFFSET  8

#define MOMENT__CTTS_ENTRY_SIZE   8
#define MOMENT__CTTS_DATA_OFFSET  8

#define MOMENT__STSZ_ENTRY_SIZE   4
#define MOMENT__STSZ_DATA_OFFSET 12

#define MOMENT__STSC_ENTRY_SIZE  12
#define MOMENT__STSC_DATA_OFFSET  8

#define MOMENT__STCO_ENTRY_SIZE   4
#define MOMENT__STCO_DATA_OFFSET  8

#define MOMENT__MP4_MAX_SAMPLE_DESCS  4096
#define MOMENT__MP4_MAX_AAC_CDATA_LEN 4096
#define MOMENT__MP4_MAX_AVC_CDATA_LEN 4096
#define MOMENT__MP4_MAX_SAMPLE_SIZE   33554432 /* 32 MB */


namespace Moment {

static LogGroup libMary_logGroup_mp4_reader ("mp4_reader", LogLevel::I);

Result
Mp4Reader::readBytes (Memory const mem)
{
    Size nread = 0;
    {
        IoResult const res = file.readFull (mem, &nread);
        if (res != IoResult::Normal) {
            if (res == IoResult::Error) {
                logE (mp4_reader, _this_func, "file \"", filename, "\" read() failed: ", exc->toString());
            } else {
                assert (res == IoResult::Eof);
                logE (mp4_reader, _this_func, "file \"", filename, "\": unexpected eof");
            }

            return Result::Failure;
        }
    }
    if (nread > mem.len()) {
        logE (mp4_reader, _this_func, "unexpected nread: ", nread);
        return Result::Failure;
    }
    if (nread != mem.len()) {
        logE (mp4_reader, _this_func, "file \"", filename, "\" unexpected eof");
        return Result::Failure;
    }

    return Result::Success;
}

// TODO parse 'uuid'
Mp4Reader::ReadBoxResult
Mp4Reader::readBoxHeader (Memory        const box_header_mem,
                          Uint64        const max_box_size,
                          Uint64      * const mt_nonnull ret_box_size,
                          Uint64      * const mt_nonnull ret_content_offset,
                          ConstMemory * const mt_nonnull ret_box_type)
{
    assert (box_header_mem.len() >= 16);
    Byte * const box_header = box_header_mem.buf();

    if (max_box_size < 8) {
        logD (mp4_reader, _this_func, "end of box");
        return ReadBoxResult_EndOfBox;
    }

    if (!readBytes (Memory (box_header, 8))) {
        logE (mp4_reader, _this_func, "readBytes() failed");
        return ReadBoxResult_Error;
    }

    *ret_box_size = readBe32 (box_header);
    *ret_content_offset = 8;
    *ret_box_type = ConstMemory (box_header + 4, 4);

    if (*ret_box_size == 1) {
        if (max_box_size < 16) {
            logD (mp4_reader, _this_func, "end of box");
            return ReadBoxResult_EndOfBox;
        }

        if (!readBytes (Memory (box_header + 8, 8))) {
            logE (mp4_reader, _this_func, "readBytes() failed");
            return ReadBoxResult_Error;
        }

        *ret_box_size = readBe64 (box_header + 8);
        *ret_content_offset = 16;
    }

    if (*ret_box_size < *ret_content_offset) {
        logE (mp4_reader, _this_func,"box_size ", *ret_box_size, " < content_offset ", *ret_content_offset);
        return ReadBoxResult_Error;
    }

    return ReadBoxResult_Normal;
}

Result
Mp4Reader::getNextBox (Box const &top_box,
                       Uint64    * const mt_nonnull offset /* current offset within top_box content */,
                       Memory      const ret_box_type,
                       Box       * const mt_nonnull ret_box)
{
    ret_box->setNull ();

    // (top_box.offset + top_box.content_offset) is supposed to be valid.
    if (top_box.offset + top_box.content_offset + *offset < top_box.offset + top_box.content_offset) {
        logE (mp4_reader, _this_func, "integer overflow: "
               "top_box.offset ", top_box.offset, ", top_box.content_offset ", top_box.content_offset);
        return Result::Failure;
    }
    Uint64 const pos = top_box.offset + top_box.content_offset + *offset;
    Uint64 const max_box_size = top_box.size - top_box.content_offset - *offset;

    logD (mp4_reader, _this_func, "iteration: pos ", pos, ", top_box.offset+top_box.size ", top_box.offset + top_box.size);

    if (!file.seek (pos, SeekOrigin::Beg)) {
        logE (mp4_reader, _this_func, "file.seek() failed: ", exc->toString());
        return Result::Failure;
    }

    Byte box_header [16];
    Uint64 box_size;
    Uint64 content_offset;
    ConstMemory box_type;
    {
        ReadBoxResult const res = readBoxHeader (Memory::forObject (box_header), max_box_size, &box_size, &content_offset, &box_type);
        if (res == ReadBoxResult_Error) {
            logE (mp4_reader, _this_func, "readBoxHeader() failed");
            return Result::Failure;
        } else
        if (res == ReadBoxResult_EndOfBox) {
            return Result::Success;
        }
        assert (res == ReadBoxResult_Normal);
    }
    logD (mp4_reader, _this_func, "box \"", box_type, "\", len ", box_size, ", offs ", pos,
           ", content_offset ", content_offset);

    logD (mp4_reader, _this_func, "found \"", box_type, "\"");

    assert (ret_box_type.len() == 4 && box_type.len() == 4);
    memcpy (ret_box_type.buf(), box_type.buf(), box_type.len());

    *ret_box = Box (pos, content_offset, box_size);
    *offset += box_size;

    return Result::Success;
}

Result
Mp4Reader::doGetBox (Box const   &top_box,
                     ConstMemory   const desired_box_type,
                     Box         * const mt_nonnull ret_box)
{
    logD (mp4_reader, _this_func, "desired: ", desired_box_type, ", top: ", top_box);

    ret_box->setNull ();

    Uint64 offset = 0;
    for (;;) {
        Box box;
        Byte box_type [4];
        if (!getNextBox (top_box, &offset, Memory::forObject (box_type), &box)) {
            logE (mp4_reader, _this_func, "getNextBox() failed");
            return Result::Failure;
        }
        if (box.isNull())
            break;

        if (equal (ConstMemory::forObject (box_type), desired_box_type)) {
            logD (mp4_reader, _this_func, "found \"", ConstMemory::forObject (box_type), "\"");
            *ret_box = box;
            return Result::Success;
        }
    }

    assert (ret_box->isNull());
    logD (mp4_reader, _this_func, "\"", desired_box_type, "\" not found");
    return Result::Success;
}

template <class ...Args>
Result Mp4Reader::getBox (Box const   &top_box,
                          ConstMemory   const desired_box_type,
                          Box         * const mt_nonnull ret_box,
                          Args const  &...args)
{
    if (!doGetBox (top_box, desired_box_type, ret_box)) {
        logE (mp4_reader, args..., "doGetBox (", desired_box_type, " failed");
        return Result::Failure;
    }
    if (ret_box->isNull()) {
        logE (mp4_reader, args..., "no ", desired_box_type);
        return Result::Failure;
    }

    logD (mp4_reader, args..., desired_box_type, ": ", *ret_box);
    return Result::Success;
}

template <class T, class ...Args>
static bool addOverflows (T const left, T const right, Args const &...args)
{
    if (left + right < left) {
        logE (mp4_reader, args..., "integer overflow");
        return true;
    }

    return false;
}

Result
Mp4Reader::prepareTable (Table     * const mt_nonnull table,
                         Box const &box,
                         Size        const table_entry_size,
                         Size        const data_offset)
{
    table->page_pool = page_pool;
    table->page = page_pool->grabPage ();
    table->page->data_len = 0;
    table->page_pos = 0;
    table->pos = 0;

    if (data_offset > box.size - box.content_offset) {
        logE (mp4_reader, _this_func, "the box is too small");
        return Result::Failure;
    }

    if (addOverflows<Uint64> (box.offset + box.content_offset, data_offset, _this_func))
        return Result::Failure;

    assert (data_offset >= 4);
    if (!file.seek (box.offset + box.content_offset + data_offset - 4, SeekOrigin::Beg)) {
        logE (mp4_reader, _this_func, "file.seek() failed: ", exc->toString());
        return Result::Failure;
    }

    {
        Byte buf [4];
        if (!readBytes (Memory::forObject (buf))) {
            logE (mp4_reader, _this_func, "readBytes() failed");
            return Result::Failure;
        }
        table->num_entries = readBe32 (buf);
        if ((Uint64) table->num_entries * table_entry_size > box.size - box.content_offset - data_offset) {
            logW (mp4_reader, _this_func,
                   "inconsistent num_entries ", table->num_entries, ", "
                   "table_entry_size ", table_entry_size, ", "
                   "data size ", box.size - box.content_offset - data_offset);
            table->num_entries = (box.size - box.content_offset - data_offset) / table_entry_size;
        }
    }

    // !table->box.isNull() means that the box is present and valid.
    table->box = box;

    // Speculative readTableIfNeeded() is probably a good idea
    // because we've just seeked to the right location.
    if (readTableIfNeeded (table, table_entry_size, data_offset) == ReadResult::Error) {
        logE (mp4_reader, _this_func, "readTableIfNeeded() failed");
        table->box.setNull ();
        return Result::Failure;
    }

    return Result::Success;
}

// Ensures that the next table entry is available in table->page.
VodReader::ReadResult
Mp4Reader::readTableIfNeeded (Table * const mt_nonnull table,
                              Size    const table_entry_size,
                              Size    const data_offset)
{
    logD (mp4_reader, _this_func,
           "table 0x", fmt_hex, (UintPtr) table, fmt_def, ", "
           "pos ", table->pos, ", "
           "page_pos ", table->page_pos, ", "
           "table_entry_size ", table_entry_size, ", "
           "page->data_len ", table->page->data_len);

    if (table->pos + table_entry_size <= table->page->data_len) {
        logD (mp4_reader, _this_func, "no-op");
        return ReadResult::Normal;
    }

    if ((Uint64) table->num_entries * table_entry_size <
                table->page_pos + table->page->data_len + table_entry_size)
    {
        logD (mp4_reader, _this_func, "out of table entries, returning ReadResult::NoMoreFrames");
        return ReadResult::NoMoreFrames;
    }

    Uint64 toread = (Uint64) table->num_entries * table_entry_size -
                    (table->page_pos + table->page->data_len);
    {
        Uint64 const max_len = page_pool->getPageSize() - page_pool->getPageSize() % table_entry_size;
        if (toread > max_len)
            toread = max_len;
    }

    logD (mp4_reader, _this_func,
           "offset ", table->box.offset, ", "
           "content_offset ", table->box.content_offset, ", "
           "data_offset ", data_offset, ", "
           "page_pos ", table->page_pos, ", "
           "page->data_len ", table->page->data_len);

    if (!file.seek (  table->box.offset
                    + table->box.content_offset
                    + data_offset
                    + table->page_pos
                    + table->page->data_len,
                    SeekOrigin::Beg))
    {
        logE (mp4_reader, _this_func, "file.seek() failed: ", exc->toString());
        return ReadResult::Error;
    }

    if (!readBytes (Memory (table->page->getData(), toread))) {
        logE (mp4_reader, _this_func, "readBytes() failed");
        return ReadResult::Error;
    }

    table->pos = 0;
    table->page_pos += table->page->data_len;
    table->page->data_len = toread;

    logD (mp4_reader, _this_func, "table 0x", fmt_hex, (UintPtr) table, fmt_def, ", page->data_len ", table->page->data_len);

    return ReadResult::Normal;
}

Result
Mp4Reader::parseDescriptorHead (Uint64 * const ret_body_offset,
                                Byte   * const ret_tag,
                                Uint64 * const ret_size)
{
    Byte buf [2];
    if (!readBytes (Memory (buf, 2))) {
        logE (mp4_reader, _this_func, "readBytes() failed");
        return Result::Failure;
    }
    *ret_tag = buf [0];
    *ret_body_offset = 2;

    // 14496-1 expandable class size (8.3.3)
    //
    // int sizeOfInstance = 0
    // bit(1) nextByte;
    // bit(7) sizeOfInstance;
    // while (nextByte) {
    //   bit(1) nextByte;
    //   bit(7) sizeByte;
    //   sizeOfInstance = sizeOfInstance<<7 | sizeByte;
    // }

    Uint64 size = buf [1] & 0x7f;
    while (buf [1] & 0x80) {
        if ((size << 7) < size) {
            logE (mp4_reader, _this_func, "size overflow");
            return Result::Failure;
        }

        size = (size << 7) | (buf [1] & 0x7f);
        ++*ret_body_offset;

        // Protect against reading a very long sequence of zero bytes.
        if (*ret_body_offset > 128 /* arbitrary large enough number */) {
            logE (mp4_reader, _this_func, "bad sizeOfInstance");
            return Result::Failure;
        }

        if (!readBytes (Memory (buf + 1, 1))) {
            logE (mp4_reader, _this_func, "readBytes() failed");
            return Result::Failure;
        }
    }
    *ret_size = size;

    return Result::Success;
}

Result
Mp4Reader::parseAudioEsDescriptor (Size const sample_desc_idx)
{
    /* 14496-1 2010

       BaseDescriptor is empty.
       Note that every "class" has a 1-byte tag at the beginning and
       a size spec.

       class ES_Descriptor extends BaseDescriptor : bit(8) tag=ES_DescrTag {
         bit(16) ES_ID;
         bit(1) streamDependenceFlag;
         bit(1) URL_Flag;
         bit(1) OCRstreamFlag;
         bit(5) streamPriority;
         if (streamDependenceFlag)
           bit(16) ependsOn_ES_ID;
         if (UR_Flag) {
           bit(8) URLlength;
           bit(8) URLstring[URLlength];
         }
         if (OCRstreamFlag)
           bit(16) OCR_ES_Id;
         DecoderConfigDescriptor decConfigDescr;
         if (ODProfileLevelIndication==0x01) // no SL extension.
         {
           SLConfigDescriptor slConfigDescr;
         }
         else                                // SL extension is possible.
         {
           SLConfigDescriptor slConfigDescr;
         }
         IPI_DescrPointer ipiPtr[0 .. 1];
         IP_IdentificationDataSet ipIDS[0 .. 255];
         IPMP_DescriptorPointer ipmpDescrPtr[0 .. 255];
         LaunguageDescriptor langDescr[0 .. 255];
         QoS_Descriptor qosDescr[0 .. 1];
         RegistrationDescriptor regDescr[0 .. 1];
         ExtensionDescriptor extDescr[0 .. 255];
       }
    */

    {
        Uint64 body_offset;
        Byte   tag;
        Uint64 size;
        if (!parseDescriptorHead (&body_offset, &tag, &size)) {
            logE (mp4_reader, _this_func, "parseDescriptorHead() failed");
            return Result::Failure;
        }

        if (tag != 0x03) {
            logE (mp4_reader, _this_func, "no ES_Descriptor, tag ", (unsigned) tag);
            return Result::Failure;
        }
    }

    {
        Byte buf [3];
        if (!readBytes (Memory (buf, 3))) {
            logE (mp4_reader, _this_func, "readBytes() failed");
            return Result::Failure;
        }

        bool const stream_dependence_flag = buf [2] & 0x80;
        bool const url_flag               = buf [2] & 0x40;
        bool const ocr_stream_flag        = buf [2] & 0x20;

        if (stream_dependence_flag) {
            if (!file.seek (2, SeekOrigin::Cur)) {
                logE (mp4_reader, _this_func, "seek() failed");
                return Result::Failure;
            }
        }

        if (url_flag) {
            Byte url_len;
            if (!readBytes (Memory::forObject (url_len))) {
                logE (mp4_reader, _this_func, "readBytes() failed");
                return Result::Failure;
            }

            if (url_len > 0) {
                if (!file.seek (url_len, SeekOrigin::Cur)) {
                    logE (mp4_reader, _this_func, "seek() failed");
                    return Result::Failure;
                }
            }
        }

        if (ocr_stream_flag) {
            if (!file.seek (2, SeekOrigin::Cur)) {
                logE (mp4_reader, _this_func, "seek() failed");
                return Result::Failure;
            }
        }
    }

    /* 14496-1 2010
       class DecoderConfigDescriptor extends BaseDescriptor : bit(8) tag=DescoderConfigDescrTag
       {
         bit(8) objectTypeIndication;
         bit(6) streamType;
         bit(1) upStream;
         const bit(1) reserved=1;
         bit(24) bufferSizeDB;
         bit(32) maxBitrate;
         bit(32) avgBitrate;
         DecoderSpecificInfo decSpecificInfo[0 .. 1];
         profileLevelIndicationIndexDescriptor profileLevelIndicationIndexDesc[0 .. 255];
       }
    */

    {
        Uint64 body_offset;
        Byte   tag;
        Uint64 size;
        if (!parseDescriptorHead (&body_offset, &tag, &size)) {
            logE (mp4_reader, _this_func, "parseDescriptorHead() failed");
            return Result::Failure;
        }

        if (tag != 0x04) {
            logE (mp4_reader, _this_func, "no DecoderConfigDescriptor");
            return Result::Failure;
        }
    }

    {
        Byte buf [13];
        if (!readBytes (Memory (buf, 13))) {
            logE (mp4_reader, _this_func, "bad DecoderConfigDescriptor");
            return Result::Failure;
        }

        Byte const object_type_indication = buf [0];
        if (object_type_indication != 0x40)
            logW (mp4_reader, _this_func, "audio objectTypeIndication is not ISO/IEC 14496-3");

        Byte const stream_type = (buf [1] >> 2);
        if (stream_type != 0x05)
            logW (mp4_reader, _this_func, "audio steamType is not 0x05");
    }

    Size cdata_len = 0;
    {
        Uint64 body_offset;
        Byte   tag;
        Uint64 size;
        if (!parseDescriptorHead (&body_offset, &tag, &size)) {
            logE (mp4_reader, _this_func, "parseDescriptorHead() failed");
            return Result::Failure;
        }

        if (tag != 0x05) {
            logE (mp4_reader, _this_func, "no DecoderSpecificInfo");
            return Result::Failure;
        }

        cdata_len = size;
    }

    {
        if (cdata_len > MOMENT__MP4_MAX_AAC_CDATA_LEN) {
            logE (mp4_reader, _this_func,
                   "AAC codec data is too long: ", cdata_len, ", "
                   "max is ", MOMENT__MP4_MAX_AAC_CDATA_LEN);
            return Result::Failure;
        }

        aac_sample_descs [sample_desc_idx].aac_cdata_buf = new (std::nothrow) Byte [cdata_len];
        assert (aac_sample_descs [sample_desc_idx].aac_cdata_buf);

        aac_sample_descs [sample_desc_idx].aac_cdata_len = (Size) cdata_len;
        if (!readBytes (Memory (aac_sample_descs [sample_desc_idx].aac_cdata_buf,
                                aac_sample_descs [sample_desc_idx].aac_cdata_len)))
        {
            logE (mp4_reader, _this_func, "readBytes() failed");
            return Result::Failure;
        }

        if (logLevelOn (mp4_reader, LogLevel::Debug)) {
            logLock ();
            logD_locked_ (_this_func, "stsd[", sample_desc_idx, "] AAC codec data:");
            logHex_locked_ (LogLevel::Debug,
                            ConstMemory (aac_sample_descs [sample_desc_idx].aac_cdata_buf,
                                         aac_sample_descs [sample_desc_idx].aac_cdata_len),
                            _this_func);
            logUnlock ();
        }
    }

    return Result::Success;

    /* 14496-3 2005

       extends DecoderSpecificInfo

       AudioSpecificConfig ()
       {
         audioObjectType = GetAudioObjectType();
         samplingFrequencyIndex; 4
         if (samplingFrequencyIndex == 0xf) {
           samplingFrequency; 24
         }
         channelConfiguration; 4

         sbrPresentFlag = -1;
         if (audioObjectType == 5) {
           extensionAudioObjectType = audioObjectType;
           sbrPresentFlag = 1;
           extensionSamplingFrequencyIndex; 4
           if (extensionSamplingFrequencyIndex == 0xf)
             extensionSamplingFrequency; 24
           audioObjectType = GetAudioObjectType();
         }
         else
         {
           extensionAudioObjectType = 0;
         }

         switch (audioObjectType) {
           // ...
           case 2:
             GASpecificConfig();
             break;
           // ...
         }

         // ...
       }

       GetAudioObjectType()
       {
         audioObjectType; 5
         if (audioObjectType == 31) {
           audioObjectType = 32 + audioObjectTypeExt; 6
         }
         return audioObjectType;
       }

       GASpecificConfig (samplingFrequencyIndex,
                         channelConfiguration,
                         audioObjectType)
       {
         frameLengthFlag; 1
         dependsOnCoreCoder; 1
         if (dependsOnCoreCoder) {
           coreCoderDelay; 14
         }
         extensionFlag; 1
         if (!channelConfiguration) {
           program_config_element();
         }
         if ((audioObjectType == 6) || (audioObjectType == 20)) {
           layerNr; 3
         }
         if (extensionFlag) {
           if (audioObjectType == 22) {
             numOfSubFrame; 5
             layer_length; 11
           }
           if (audioObjectType == 17 ||
               audioObjectType == 19 ||
               audioObjectType == 20 ||
               audioObjectType == 23)
           {
             aacSectionDataResilienceFlag; 1
             aacScalefactorDataResilienceFlag; 1
             aacSpectralDataResilienceFlag; 1
           }
           extensionFlag3; 1
           if (extensionFlag3) {
             // tbd in version 3
           }
         }
       }
    */
}

Result
Mp4Reader::parseAudioSampleEntry (ConstMemory const sample_entry_box_type,
                                  Uint64      const sample_entry_box_size,
                                  Uint64      const sample_entry_content_offset,
                                  Size        const sample_desc_idx)
{
    /* 14496-12

       aligned(8) abstract class SampleEntry (unsigned int(32) format) extends Box(format) {
         const unsigned int(8)[6] reserved = 0;
         unsigned int(16) data_reference_index;
       }

       class AudioSampleEntry(codingname) extends SampleEntry (codingname) {
         const unsigned int(32)[2] reserved = 0;
         template unsigned int(16) channelcount = 2;
         template unsigned int(16) samplesize = 16;
         unsigned int(16) pre_defined = 0;
         const unsigned int(16) reserved = 0 ;
         template unsigned int(32) samplerate = {timescale of media}<<16;
       }

       total 28 bytes
    */

    if (!equal (sample_entry_box_type, "mp4a")) {
        logE (mp4_reader, _this_func, "unsupported audio coding \"", sample_entry_box_type, "\"");
        return Result::Failure;
    }

    if (sample_entry_box_size - sample_entry_content_offset < 28) {
        logE (mp4_reader, _this_func, "invalid audio sample entry");
        return Result::Failure;
    }

    if (!file.seek (28, SeekOrigin::Cur)) {
        logE (mp4_reader, _this_func, "file.seek() failed: ", exc->toString());
        return Result::Failure;
    }

    {
        Byte box_header [16];
        Uint64 box_size;
        Uint64 content_offset;
        ConstMemory box_type;
        {
            Uint64 const max_box_size = sample_entry_box_size - sample_entry_content_offset - 28;
            ReadBoxResult const res = readBoxHeader (Memory::forObject (box_header),
                                                     max_box_size,
                                                     &box_size,
                                                     &content_offset,
                                                     &box_type);
            if (res == ReadBoxResult_Error) {
                logE (mp4_reader, _this_func, "readBoxHeader() failed");
                return Result::Failure;
            } else
            if (res == ReadBoxResult_EndOfBox) {
                logD (mp4_reader, _this_func, "EndOfBox while looking for esds");
                return Result::Failure;
            }
            assert (res == ReadBoxResult_Normal);
        }

        if (!equal (box_type, "esds")) {
            logE (mp4_reader, _this_func, "esds box expected");
            return Result::Failure;
        }

        // skipping version and flags
        if (!file.seek (4, SeekOrigin::Cur)) {
            logE (mp4_reader, _this_func, "file.seek() failed");
            return Result::Failure;
        }
    }

    if (!parseAudioEsDescriptor (sample_desc_idx)) {
        logE (mp4_reader, _this_func, "parseAudioEsDescriptor() failed");
        return Result::Failure;
    }

    return Result::Success;
}

Result
Mp4Reader::parseVisualSampleEntry (ConstMemory const sample_entry_box_type,
                                   Uint64      const sample_entry_box_size,
                                   Uint64      const sample_entry_content_offset,
                                   Size        const sample_desc_idx)
{
    /* 14496-12 VisualSampleEntry

        6 const unsigned int(8)[6] reserved = 0;
        2 unsigned int(16) data_reference_index;

        2 unsigned int(16) pre_defined = 0;
        2 const unsigned int(16) reserved = 0;
        12 unsigned int(32)[3] pre_defined = 0;
        2 unsigned int(16) width;
        2 unsigned int(16) height;
        4 template unsigned int(32) horizresolution = 0x00480000; // 72 dpi
        4 template unsigned int(32) vertresolution = 0x00480000; // 72 dpi
        4 const unsigned int(32) reserved = 0;
        2 template unsigned int(16) frame_count = 1;
        32 string[32] compressorname;
        2 template unsigned int(16) depth = 0x0018;
        2 int(16) pre_defined = -1;

        total 78 bytes

        then follows 14496-15 AVCConfigurationBox
    */

    if (!equal (sample_entry_box_type, "avc1")) {
        logE (mp4_reader, _this_func, "unsupported video coding \"", sample_entry_box_type, "\"");
        return Result::Failure;
    }

    if (sample_entry_box_size - sample_entry_content_offset < 78) {
        logE (mp4_reader, _this_func, "invalid video sample entry");
        return Result::Failure;
    }

    if (!file.seek (78, SeekOrigin::Cur)) {
        logE (mp4_reader, _this_func, "file.seek() failed: ", exc->toString());
        return Result::Failure;
    }

    Uint64 offset = 0;
    bool got_avcC = false;
    for (;;) {
        if (offset >= sample_entry_box_size - sample_entry_content_offset - 78)
            break;

        Byte box_header [16];
        Uint64 box_size;
        Uint64 content_offset;
        ConstMemory box_type;
        {
            Uint64 const max_box_size = sample_entry_box_size - sample_entry_content_offset - 78 - offset;
            ReadBoxResult const res = readBoxHeader (Memory::forObject (box_header),
                                                     max_box_size,
                                                     &box_size,
                                                     &content_offset,
                                                     &box_type);
            if (res == ReadBoxResult_Error) {
                logE (mp4_reader, _this_func, "readBoxHeader() failed");
                return Result::Failure;
            } else
            if (res == ReadBoxResult_EndOfBox) {
                logD (mp4_reader, _this_func, "EndOfBox while looking for avcC");
                break;
            }
            assert (res == ReadBoxResult_Normal);
        }

        logD (mp4_reader, _this_func, "box_type \"", box_type, "\"");
        if (equal (box_type, "avcC")) {
            logD (mp4_reader, _this_func, "avcC box, size ", box_size, ", content_offset ", content_offset);

            Uint64 const cdata_len = box_size - content_offset;
            if (cdata_len > MOMENT__MP4_MAX_AVC_CDATA_LEN) {
                logE (mp4_reader, _this_func,
                      "AVC codec data is too long: ", cdata_len, ", "
                      "max is ", MOMENT__MP4_MAX_AVC_CDATA_LEN);
                return Result::Failure;
            }

            avc_sample_descs [sample_desc_idx].avc_cdata_buf = new (std::nothrow) Byte [cdata_len];
            assert (avc_sample_descs [sample_desc_idx].avc_cdata_buf);

            avc_sample_descs [sample_desc_idx].avc_cdata_len = (Size) cdata_len;
            if (!readBytes (Memory (avc_sample_descs [sample_desc_idx].avc_cdata_buf,
                                    avc_sample_descs [sample_desc_idx].avc_cdata_len)))
            {
                logE (mp4_reader, _this_func, "readBytes() failed");
                return Result::Failure;
            }

            if (logLevelOn (mp4_reader, LogLevel::Debug)) {
                logLock ();
                logD_locked_ (_this_func, "stsd[", sample_desc_idx, "] AVC codec data:");
                logHex_locked_ (LogLevel::Debug,
                                ConstMemory (avc_sample_descs [sample_desc_idx].avc_cdata_buf,
                                             avc_sample_descs [sample_desc_idx].avc_cdata_len),
                                _this_func);
                logUnlock ();
            }

            got_avcC = true;
            break;
        }

        if (addOverflows (offset, box_size, _this_func))
            return Result::Failure;

        offset += box_size;

        if (!file.seek (box_size - content_offset, SeekOrigin::Cur)) {
            logE (mp4_reader, _this_func, "file.seek() failed: ", exc->toString());
            return Result::Failure;
        }
    }

    if (!got_avcC) {
        logE (mp4_reader, _this_func, "missing avcC box");
        return Result::Failure;
    }

    return Result::Success;
}

Result
Mp4Reader::parseStsd (Track     * const /* track */ /* TODO make use of 'track'? */,
                      Box const &stsd_box,
                      bool        const is_audio)
{
    if (   ( is_audio && aac_sample_descs)
        || (!is_audio && avc_sample_descs))
    {
        logE (mp4_reader, _this_func, "duplicate stsd box");
        return Result::Failure;
    }

    Uint32 entry_count = 0;
    {
        Byte buf [8];
        if (!readBytes (Memory::forObject (buf))) {
            logE (mp4_reader, _this_func, "readBytes() failed");
            return Result::Failure;
        }
        entry_count = readBe32 (buf + 4);
        logD (mp4_reader, _this_func, "entry_count: ", entry_count);
    }

    if (entry_count > MOMENT__MP4_MAX_SAMPLE_DESCS) {
        logE (mp4_reader, _this_func, "too many stsd entries: ", entry_count, ", max is ", MOMENT__MP4_MAX_SAMPLE_DESCS);
        return Result::Failure;
    }

    if (is_audio) {
        aac_sample_descs = new (std::nothrow) AacSampleDescription [entry_count];
        assert (aac_sample_descs);
        num_aac_sample_descs = entry_count;
    } else {
        avc_sample_descs = new (std::nothrow) AvcSampleDescription [entry_count];
        assert (avc_sample_descs);
        num_avc_sample_descs = entry_count;
    }

    Uint64 sample_entry_offset = 8;
    for (Uint32 i = 0; i < entry_count; ++i) {
        if (sample_entry_offset >= stsd_box.size - stsd_box.content_offset) {
            logE (mp4_reader, _this_func, "missing stsd entry");
            return Result::Failure;
        }

        Byte sample_entry_box_header [16];
        Uint64 sample_entry_box_size;
        Uint64 sample_entry_content_offset;
        ConstMemory sample_entry_box_type;
        {
            Uint64 const max_box_size = stsd_box.size - stsd_box.content_offset - sample_entry_offset;
            ReadBoxResult const res = readBoxHeader (Memory::forObject (sample_entry_box_header),
                                                     max_box_size,
                                                     &sample_entry_box_size,
                                                     &sample_entry_content_offset,
                                                     &sample_entry_box_type);
            if (res == ReadBoxResult_Error) {
                logE (mp4_reader, _this_func, "readBoxHeader() failed");
                return Result::Failure;
            } else
            if (res == ReadBoxResult_EndOfBox) {
                logE (mp4_reader, _this_func, "missing stsd entry");
                return Result::Failure;
            }
            assert (res == ReadBoxResult_Normal);
        }

        logD (mp4_reader, _this_func, "coding: ", sample_entry_box_type);

        if (is_audio) {
            if (!parseAudioSampleEntry (sample_entry_box_type,
                                        sample_entry_box_size,
                                        sample_entry_content_offset,
                                        i /* sample_desc_idx */))
            {
                logE (mp4_reader, _this_func, "parseAudioSampleEntry() failed");
                return Result::Failure;
            }
        } else {
            if (!parseVisualSampleEntry (sample_entry_box_type,
                                         sample_entry_box_size,
                                         sample_entry_content_offset,
                                         i /* sample_desc_idx */))
            {
                logE (mp4_reader, _this_func, "parseVisualSampleEntry() failed");
                return Result::Failure;
            }
        }

        if (addOverflows (sample_entry_offset, sample_entry_box_size, _this_func))
            return Result::Failure;

        sample_entry_offset += sample_entry_box_size;

        if (!file.seek (sample_entry_offset + stsd_box.offset + stsd_box.content_offset,
                        SeekOrigin::Beg))
        {
            logE (mp4_reader, _this_func, "file.seek() failed: ", exc->toString());
            return Result::Failure;
        }
    }

    return Result::Success;
}

Result
Mp4Reader::parseTrack (Box const &trak_box)
{
    Box mdia_box;
    if (!getBox (trak_box, "mdia", &mdia_box, _this_func))
        return Result::Failure;

    Uint32 mdhd_timescale = 1;
    Uint64 mdhd_duration = 0;
    {
        Box mdhd_box;
        if (!getBox (mdia_box, "mdhd", &mdhd_box, _this_func))
            return Result::Failure;

        Byte buf [4];
        if (!readBytes (Memory::forObject (buf))) {
            logE (mp4_reader, _this_func, "readBytes() failed");
            return Result::Failure;
        }

        Byte const version = buf [0];
        if (version == 0) {
            Byte buf [16];
            if (!readBytes (Memory::forObject (buf))) {
                logE (mp4_reader, _this_func, "readBytes() failed");
                return Result::Failure;
            }

            mdhd_timescale = readBe32 (buf +  8);
            mdhd_duration  = readBe32 (buf + 12);
        } else {
            if (version != 1)
                logW (mp4_reader, _this_func, "unknown mdhd version: ", version);

            Byte buf [28];
            if (!readBytes (Memory::forObject (buf))) {
                logE (mp4_reader, _this_func, "readBytes() failed");
                return Result::Failure;
            }

            mdhd_timescale = readBe32 (buf + 16);
            mdhd_duration  = readBe64 (buf + 20);
        }

        logD (mp4_reader, _this_func, "mdhd_timescale ", mdhd_timescale, ", mdhd_duration ", mdhd_duration);

        if (mdhd_timescale == 0) {
          // We're going to divide by mdhd_timescale,
          // hence we don't want it to be zero.
            logW (mp4_reader, _this_func, "patching zero mdhd time scale");
            mdhd_timescale = 1;
        }
    }

    bool is_audio = false;
    Track *track = NULL;
    {
        Box hdlr_box;
        if (!getBox (mdia_box, "hdlr", &hdlr_box, _this_func))
            return Result::Failure;

        Byte buf [16];
        if (!readBytes (Memory::forObject (buf))) {
            logE (mp4_reader, _this_func, "readBytes() failed");
            return Result::Failure;
        }

        if (!memcmp (buf + 8, "vide", 4)) {
            is_audio = false;
            track = &video_track;
        } else
        if (!memcmp (buf + 8, "soun", 4)) {
            is_audio = true;
            track = &audio_track;
        } else {
            logD (mp4_reader, _this_func, "hdlr handler_type is not \"vide\", nor \"soun\"");
            logHex (mp4_reader, LogLevel::Debug, ConstMemory (buf + 8, 4), _this_func);
            return Result::Failure;
        }
    }
    assert (track);
    track->mdhd_timescale = mdhd_timescale;

    Box minf_box;
    if (!getBox (mdia_box, "minf", &minf_box, _this_func))
        return Result::Failure;

    Box stbl_box;
    if (!getBox (minf_box, "stbl", &stbl_box, _this_func))
        return Result::Failure;

    bool got_stsz = false;
    {
        Uint64 offset = 0;
        for (;;) {
            Box box;
            Byte box_type_buf [4];
            if (!getNextBox (stbl_box, &offset, Memory::forObject (box_type_buf), &box)) {
                logE (mp4_reader, _this_func, "getNextBox() failed");
                return Result::Failure;
            }
            if (box.isNull())
                break;

            ConstMemory const box_type = ConstMemory::forObject (box_type_buf);
            if (equal (box_type, "stts")) {
                do {
                    if (!prepareTable (&track->stts, box, MOMENT__STTS_ENTRY_SIZE, MOMENT__STTS_DATA_OFFSET)) {
                        logE (mp4_reader, _this_func, "prepareTable() failed for stts");
                        break;
                    }
                } while (0);
            } else
            if (equal (box_type, "ctts")) {
                do {
                    if (!prepareTable (&track->ctts, box, MOMENT__CTTS_ENTRY_SIZE, MOMENT__CTTS_DATA_OFFSET)) {
                        logE (mp4_reader, _this_func, "prepareTable() failed for ctts");
                        break;
                    }
                } while (0);
            } else
            if (equal (box_type, "stss")) {
                do {
                    if (!prepareTable (&track->stss, box, 4 /* table_entry_size */, 8 /* data_offset */)) {
                        logE (mp4_reader, _this_func, "prepareTable() failed for stss");
                        break;
                    }
                } while (0);
            } else
            if (equal (box_type, "stsd")) {
                if (!parseStsd (track, box, is_audio)) {
                    logE (mp4_reader, _this_func, "parseStsd() failed");
                    break;
                }
            } else
            if (equal (box_type, "stsz")) {
                do {
                    track->stsz.box = box;
                    {
                        Byte buf [8];
                        if (!readBytes (Memory::forObject (buf))) {
                            logE (mp4_reader, _this_func, "readBytes() failed");
                            return Result::Failure;
                        }
                        track->stsz_sample_size = readBe32 (buf);
                    }
                    if (track->stsz_sample_size == 0) {
                        if (!prepareTable (&track->stsz, box, MOMENT__STSZ_ENTRY_SIZE, MOMENT__STSZ_DATA_OFFSET)) {
                            logE (mp4_reader, _this_func, "prepareTable() failed for stsz");
                            break;
                        }

                        logD (mp4_reader, _this_func, "stsz:");
                        logHex (mp4_reader, LogLevel::Debug, track->stsz.page->mem(), _this_func);
                    }
                    got_stsz = true;
                } while (0);
            } else
            if (equal (box_type, "stsc")) {
                do {
                    if (!prepareTable (&track->stsc, box, MOMENT__STSC_ENTRY_SIZE, MOMENT__STSC_DATA_OFFSET)) {
                        logE (mp4_reader, _this_func, "prepareTable() failed for stsc");
                        break;
                    }

                    logD (mp4_reader, _this_func, "stsc:");
                    logHex (mp4_reader, LogLevel::Debug, track->stsc.page->mem(), _this_func);
                } while (0);
            } else
            if (equal (box_type, "stco")) {
                do {
                    if (!prepareTable (&track->stco, box, MOMENT__STCO_ENTRY_SIZE, MOMENT__STCO_DATA_OFFSET)) {
                        logE (mp4_reader, _this_func, "prepareTable() failed for stco");
                        break;
                    }
                } while (0);
            }

            if (   !track->stts.box.isNull()
                && !track->ctts.box.isNull()
                && !track->stss.box.isNull()
                && got_stsz
                && !track->stsc.box.isNull()
                && !track->stco.box.isNull())
            {
                break;
            }
        }
    }

    {
        bool box_missing = false;;

        if (track->stts.box.isNull()) {
            logE (mp4_reader, _this_func, "no stts");
            box_missing = true;
        }
        /* stss is optional
         *
        if (track->stss.box.isNull()) {
            logE (mp4_reader, _this_func, "no stss");
            box_missing = true;
        }
        */
        if (!got_stsz) {
            logE (mp4_reader, _this_func, "no stsz");
            box_missing = true;
        }
        if (track->stsc.box.isNull()) {
            logE (mp4_reader, _this_func, "no stsc");
            box_missing = true;
        }
        if (track->stco.box.isNull()) {
            logE (mp4_reader, _this_func, "no stco");
            box_missing = true;
        }

        if (box_missing)
            return Result::Failure;
    }

#if 0
// Unnecessary
    {
        ReadResult const res = readStscIfNeeded (tarck, true /* read_stco */);
        if (res != ReadResult::Normal)
            return Result::Failure;
    }
#endif

    if (track == &audio_track) {
        got_audio_track = true;
    } else {
        assert (track == &video_track);
        got_video_track = true;
    }

    logD (mp4_reader, _this_func, "stsc 0x", fmt_hex, (UintPtr) &track->stsc);

    return Result::Success;
}

Result
Mp4Reader::parseHeader ()
{
    logD (mp4_reader, _this_func_);

    // ftyp
    // mdat
    // moov
    //   mvhd
    //   trak
    //     tkhd
    //     mdia
    //       mdhd
    //       hdlr
    //       minf
    //         vmhd
    //         hdlr
    //         dinf
    //         stbl
    //           stsd
    //             avc1
    //               avcC
    //           stts // time-to-sample
    //           stss // sync-sample
    //           stsc // sample-to-chunk
    //           stsz // sample-size
    //           ctts // composition-time-to-sample
    //           stco // chunk-offset

    Box moov_box;
    if (!getBox (Box (0 /* offset */, 0 /* content_offset */, (Uint64) -1 /* size */),
                 "moov",
                 &moov_box,
                 _this_func))
    {
        return Result::Failure;
    }
    logD (mp4_reader, _this_func, "moov: ", moov_box);

    Uint32 mvhd_timescale = 1;
    {
        Box mvhd_box;
        if (!getBox (moov_box, "mvhd", &mvhd_box, _this_func))
            return Result::Failure;

        logD (mp4_reader, _this_func, "mvhd: seeking to ", mvhd_box.offset + mvhd_box.content_offset);
        if (!file.seek (mvhd_box.offset + mvhd_box.content_offset, SeekOrigin::Beg)) {
            logE (mp4_reader, _this_func, "file.seek() failed: ", exc->toString());
            return Result::Failure;
        }

        Byte buf [4];
        if (!readBytes (Memory::forObject (buf))) {
            logE (mp4_reader, _this_func, "readBytes() failed");
            return Result::Failure;
        }

        Byte const version = buf [0];
        if (version == 0) {
            Byte buf [16];
            if (!readBytes (Memory::forObject (buf))) {
                logE (mp4_reader, _this_func, "readBytes() failed");
                return Result::Failure;
            }

            mvhd_timescale = readBe32 (buf + 8);
        } else {
            if (version != 1)
                logW (mp4_reader, _this_func, "unknown mvhd version: ", version);

            Byte buf [28];
            if (!readBytes (Memory::forObject (buf))) {
                logE (mp4_reader, _this_func, "readBytes() failed");
                return Result::Failure;
            }

            mvhd_timescale = readBe32 (buf + 16);
        }

        logD (mp4_reader, _this_func, "mvhd_timescale: ", mvhd_timescale);
    }

    {
        Uint64 offset = 0;
        for (;;) {
            Box box;
            Byte box_type_buf [4];
            if (!getNextBox (moov_box, &offset, Memory::forObject (box_type_buf), &box)) {
                logE (mp4_reader, _this_func, "getNextBox() failed");
                return Result::Failure;
            }
            if (box.isNull())
                break;

            ConstMemory const box_type = ConstMemory::forObject (box_type_buf);
            if (equal (box_type, "trak")) {
                if (!parseTrack (box))
                    logD (mp4_reader, _this_func, "parseTrack() failed, box: ", box);

                if (got_audio_track && got_video_track)
                    break;
            }
        }
    }

    if (!got_audio_track && !got_video_track) {
        logE (mp4_reader, _this_func, "no tracks found");
        return Result::Failure;
    }

    return Result::Success;
}

void
Mp4Reader::advanceFrame (Track * const mt_nonnull track,
                         Uint32  const sample_size)
{
    ++track->cur_sample_idx;
    ++track->cur_sample_idx_in_chunk;
    track->cur_sample_offset_in_chunk += sample_size;
    ++track->cur_sample_stts_idx;
    ++track->cur_sample_ctts_idx;
    track->cur_sample_dts_nanosec += track->cur_stts_sample_delta_nanosec;

    if (track->stsz_sample_size == 0)
        track->stsz.pos += MOMENT__STSZ_ENTRY_SIZE;
}

VodReader::ReadResult
Mp4Reader::readSttsIfNeeded (Track * const mt_nonnull track)
{
    if (track->cur_sample_stts_idx < track->cur_stts_sample_count)
        return ReadResult::Normal;

    do {
        {
            ReadResult const res = readTableIfNeeded (&track->stts,
                                                      MOMENT__STTS_ENTRY_SIZE,
                                                      MOMENT__STTS_DATA_OFFSET);
            if (res != ReadResult::Normal)
                return res;
        }

        track->cur_stts_sample_count = readBe32 (track->stts.page->getData() + track->stts.pos);
        track->cur_stts_sample_delta_nanosec =
                (Int64) (Int32) readBe32 (track->stts.page->getData() + track->stts.pos + 4)
                * 1000000000LL
                / (Int64) track->mdhd_timescale;
        track->stts.pos += MOMENT__STTS_ENTRY_SIZE;
    } while (track->cur_stts_sample_count == 0);

    track->cur_sample_stts_idx = 0;

    return ReadResult::Normal;
}

VodReader::ReadResult
Mp4Reader::readCttsIfNeeded (Track * const mt_nonnull track)
{
    if (track->cur_sample_ctts_idx < track->cur_ctts_sample_count)
        return ReadResult::Normal;

    do {
        {
            ReadResult const res = readTableIfNeeded (&track->ctts,
                                                      MOMENT__CTTS_ENTRY_SIZE,
                                                      MOMENT__CTTS_DATA_OFFSET);
            if (res != ReadResult::Normal)
                return res;
        }

        track->cur_ctts_sample_count  = readBe32 (track->ctts.page->getData() + track->ctts.pos);
        track->cur_ctts_sample_offset_nanosec =
                (Int64) (Int32) readBe32 (track->ctts.page->getData() + track->ctts.pos + 4)
                * 1000000000LL
                / (Int64) track->mdhd_timescale;
        track->ctts.pos += MOMENT__CTTS_ENTRY_SIZE;
    } while (track->cur_ctts_sample_count == 0);

    track->cur_sample_ctts_idx = 0;

    return ReadResult::Normal;
}

VodReader::ReadResult
Mp4Reader::readStscIfNeeded (Track * const mt_nonnull track)
{
    logD (mp4_reader, _this_func,
          "cur_sample_idx_in_chunk ", track->cur_sample_idx_in_chunk, ", "
          "cur_chunk_samples_per_chunk ", track->cur_chunk_samples_per_chunk, ", "
          "stsc.page_pos ", track->stsc.page_pos, ", "
          "stsc.pos ", track->stsc.pos);

    if (track->cur_sample_idx_in_chunk < track->cur_chunk_samples_per_chunk)
        return ReadResult::Normal;

  // start of a new chunk

    if (track->got_next_stsc_chunk_idx
        && track->cur_chunk_idx >= track->next_stsc_chunk_idx)
    {
      // the new chunk is described by the next stsc entry

        logD (mp4_reader, _this_func, "new stsc entry");

        do {
            logD (mp4_reader, _this_func, "stsc read iteration");

            if (track->cur_chunk_idx > track->stsc.num_entries) {
                logD (mp4_reader, _this_func, "out of stsc entries, returning ReadResult::NoMoreFrames: ",
                      "cur_chunk_idx ", track->cur_chunk_idx, ", "
                      "stsc.num_entries: ", track->stsc.num_entries);
                return ReadResult::NoMoreFrames;
            }

            if (track->stsc.num_entries * MOMENT__STSC_ENTRY_SIZE
                        >= track->stsc.page_pos + track->stsc.page->data_len + MOMENT__STSC_ENTRY_SIZE * 2)
            {
                {
                    ReadResult const res = readTableIfNeeded (&track->stsc,
                                                              MOMENT__STSC_ENTRY_SIZE * 2,
                                                              MOMENT__STSC_DATA_OFFSET);
                    if (res != ReadResult::Normal)
                        return res;
                }

                track->next_stsc_chunk_idx = readBe32 (track->stsc.page->getData() + track->stsc.pos);
                track->got_next_stsc_chunk_idx = true;
            } else {
                {
                    ReadResult const res = readTableIfNeeded (&track->stsc,
                                                              MOMENT__STSC_ENTRY_SIZE,
                                                              MOMENT__STSC_DATA_OFFSET);
                    if (res != ReadResult::Normal)
                        return res;
                }

                track->got_next_stsc_chunk_idx = false;
            }

            track->cur_chunk_samples_per_chunk = readBe32 (track->stsc.page->getData() + track->stsc.pos + 4);
            track->cur_chunk_sample_description_index = readBe32 (track->stsc.page->getData() + track->stsc.pos + 8);
            track->stsc.pos += MOMENT__STSC_ENTRY_SIZE;

            ++track->cur_chunk_idx;
        } while (track->cur_chunk_samples_per_chunk == 0);

        logD (mp4_reader, _this_func,
               "cur_chunk_samples_per_chunk ", track->cur_chunk_samples_per_chunk, ", "
               "cur_chunk_sample_description_index ", track->cur_chunk_sample_description_index, ", "
               "next_stsc_chunk_idx ", track->next_stsc_chunk_idx, ", "
               "got_next_stsc_chunk_idx: ", track->got_next_stsc_chunk_idx);
    } else {
        ++track->cur_chunk_idx;
    }

    track->cur_sample_idx_in_chunk = 0;
    track->cur_sample_offset_in_chunk = 0;

    return ReadResult::Normal;
}

VodReader::ReadResult
Mp4Reader::readStcoIfNeeded (Track * const mt_nonnull track)
{
    if (track->cur_stco_chunk_idx == track->cur_chunk_idx)
        return ReadResult::Normal;

    {
        ReadResult const res = readTableIfNeeded (&track->stco,
                                                  MOMENT__STCO_ENTRY_SIZE,
                                                  MOMENT__STCO_DATA_OFFSET);
        if (res != ReadResult::Normal)
            return res;
    }

    track->cur_chunk_offset = readBe32 (track->stco.page->getData() + track->stco.pos);
    track->stco.pos += MOMENT__STCO_ENTRY_SIZE;

    track->cur_stco_chunk_idx = track->cur_chunk_idx;

    return ReadResult::Normal;
}

VodReader::ReadResult
Mp4Reader::peekNextFrame (Track      * const mt_nonnull track,
                          SampleInfo * const mt_nonnull ret_sample_info)
{
    if (track->cur_sample_idx >= track->stsz.num_entries) {
        logD (mp4_reader, _this_func, "out of stsz entries, returning ReadResult::NoMoreFrames");
        return ReadResult::NoMoreFrames;
    }

    Uint32 sample_size = track->stsz_sample_size;
    if (sample_size == 0) {
        {
            ReadResult const res = readTableIfNeeded (&track->stsz,
                                                      MOMENT__STSZ_ENTRY_SIZE,
                                                      MOMENT__STSZ_DATA_OFFSET);
            if (res != ReadResult::Normal)
                return res;
        }

        sample_size = readBe32 (track->stsz.page->getData() + track->stsz.pos);
    }

    {
        ReadResult const res = readStscIfNeeded (track);
        if (res != ReadResult::Normal)
            return res;
    }

    {
        ReadResult const res = readStcoIfNeeded (track);
        if (res != ReadResult::Normal)
            return res;
    }

    {
        ReadResult const res = readSttsIfNeeded (track);
        if (res != ReadResult::Normal)
            return res;
    }

    if (!track->ctts.box.isNull()) {
        ReadResult const res = readCttsIfNeeded (track);
        if (res != ReadResult::Normal)
            return res;
    }

    Uint64 const sample_cts_nanosec = track->cur_sample_dts_nanosec + track->cur_ctts_sample_offset_nanosec;
    Uint64 const sample_offset = track->cur_chunk_offset + track->cur_sample_offset_in_chunk;

    logS (mp4_reader, _this_func, "SAMPLE SIZE: ", sample_size);
//#if 0
    logD (mp4_reader, _this_func, "CHUNK IDX: ", track->cur_chunk_idx);
    logD (mp4_reader, _this_func, "SAMPLE IDX IN CHUNK: ", track->cur_sample_idx_in_chunk);
    logD (mp4_reader, _this_func, "CHUNK OFFSET: ", track->cur_chunk_offset);
    logD (mp4_reader, _this_func, "SAMPLE OFFSET: ", sample_offset);
    logD (mp4_reader, _this_func, "SAMPLE DESCRIPTION INDEX: ", track->cur_chunk_sample_description_index);
    logD (mp4_reader, _this_func, "SAMPLE DTS DELTA NANOSEC: ", track->cur_stts_sample_delta_nanosec);
    logD (mp4_reader, _this_func, "SAMPLE CTS OFFSET: ", (Int64) track->cur_ctts_sample_offset_nanosec);
    logD (mp4_reader, _this_func, "SAMPLE DTS NANOSEC: ", track->cur_sample_dts_nanosec);
    logD (mp4_reader, _this_func, "SAMPLE CTS NANOSEC: ", sample_cts_nanosec);
//#endif

    ret_sample_info->sample_size = sample_size;
    ret_sample_info->sample_offset = sample_offset;
    ret_sample_info->sample_description_index = track->cur_chunk_sample_description_index;
    ret_sample_info->sample_dts_nanosec = track->cur_sample_dts_nanosec;
    ret_sample_info->sample_cts_nanosec = sample_cts_nanosec;

    if (mt_unlikely (ret_sample_info->sample_description_index == 0)) {
        logW (mp4_reader, _this_func, "patching zero sample description index");
        ret_sample_info->sample_description_index = 1;
    }

    return ReadResult::Normal;
}

VodReader::ReadResult
Mp4Reader::peekNextValidFrame (Track      * const mt_nonnull track,
                               SampleInfo * const mt_nonnull ret_sample_info)
{
    for (;;) {
        ReadResult const res = peekNextFrame (track, ret_sample_info);
        if (res != ReadResult::Normal)
            return res;

        if (ret_sample_info->sample_size >= MOMENT__MP4_MAX_SAMPLE_SIZE) {
            logW (mp4_reader, _this_func, "sample is too large, dropping: ",
                  ret_sample_info->sample_size, ", max ", MOMENT__MP4_MAX_SAMPLE_SIZE);
            continue;
        }

        break;
    }

    return ReadResult::Normal;
}

VodReader::ReadResult
Mp4Reader::readNextFrame (ReadMessage   * const mt_nonnull read_msg,
                          MediaMessage ** const mt_nonnull ret_msg,
                          bool            const stop_time_reached,
                          Time            const stop_time_microsec)
{
    logD (mp4_reader, _this_func_);

    *ret_msg = NULL;

    if (mt_unlikely (error_state)) {
        logD (mp4_reader, _this_func, "returning ReadResult::Error");
        return ReadResult::Error;
    }

    if (mt_unlikely (!header_parsed)) {
        header_parsed = true;
        if (!parseHeader ()) {
            error_state = true;
            logD (mp4_reader, _this_func, "returning ReadResult::Error");
            return ReadResult::Error;
        }
    }

    SampleInfo audio_sample_info;
    SampleInfo video_sample_info;

    ReadResult audio_res = ReadResult::NoMoreFrames;
    if (   got_audio_track
        && !audio_track.track_end_reached)
    {
        audio_res = peekNextValidFrame (&audio_track, &audio_sample_info);
        assert (audio_res != ReadResult::StopTimeReached);
    }

    ReadResult video_res = ReadResult::NoMoreFrames;
    if (   got_video_track
        && !video_track.track_end_reached)
    {
        video_res = peekNextValidFrame (&video_track, &video_sample_info);
        assert (audio_res != ReadResult::StopTimeReached);
    }

    if (mt_unlikely (   audio_res == ReadResult::NoMoreFrames
                     && video_res == ReadResult::NoMoreFrames))
    {
        logD (mp4_reader, _this_func, "NoMoreFrames");
        return ReadResult::NoMoreFrames;
    }

    if (mt_unlikely (   audio_res == ReadResult::Error
                     || video_res == ReadResult::Error))
    {
        logD (mp4_reader, _this_func, "Error");
        return ReadResult::Error;
    }

    if (mt_unlikely (audio_res == ReadResult::NoMoreFrames)) {
        logD (mp4_reader, _this_func, "audio_res: NoMoreFrames, reading video");
        return readNextVideoFrame (read_msg, ret_msg, stop_time_reached, stop_time_microsec, &video_sample_info);
    }

    if (mt_unlikely (video_res == ReadResult::NoMoreFrames)) {
        logD (mp4_reader, _this_func, "video_res: NoMoreFrames, reading audio");
        return readNextAudioFrame (read_msg, ret_msg, stop_time_reached, stop_time_microsec, &audio_sample_info);
    }

    if (audio_sample_info.sample_dts_nanosec <= video_sample_info.sample_dts_nanosec)
        return readNextAudioFrame (read_msg, ret_msg, stop_time_reached, stop_time_microsec, &audio_sample_info);
    else
        return readNextVideoFrame (read_msg, ret_msg, stop_time_reached, stop_time_microsec, &video_sample_info);

    unreachable ();
    return ReadResult::Normal;
}

Mp4Reader::MediaDescResult
Mp4Reader::getAacMediaDescForFrame (Size                   const sample_description_index,
                                    Size                   const last_reported_sample_description_index,
                                    Ref<MediaDesc_Audio> * const mt_nonnull ret_media_desc__audio)
{
    if (last_reported_sample_description_index == sample_description_index)
        return MediaDescResult_SameDesc;

    if (mt_unlikely (sample_description_index > num_aac_sample_descs)) {
        logE (mp4_reader, _this_func,
               "sample description index ", sample_description_index, " "
               "out of range, ", num_aac_sample_descs, " max");
        return MediaDescResult_NoDesc;
    }

    if (!aac_sample_descs [sample_description_index - 1].aac_cdata_buf) {
        logE (mp4_reader, _this_func, "missing AAC sample description, index ", sample_description_index);
        return MediaDescResult_NoDesc;
    }

    if (last_reported_sample_description_index != 0) {
        assert (aac_sample_descs [last_reported_sample_description_index - 1].aac_cdata_buf);
        if (equal (ConstMemory (aac_sample_descs [last_reported_sample_description_index - 1].aac_cdata_buf,
                                aac_sample_descs [last_reported_sample_description_index - 1].aac_cdata_len),
                   ConstMemory (aac_sample_descs [sample_description_index - 1].aac_cdata_buf,
                                aac_sample_descs [sample_description_index - 1].aac_cdata_len)))
        {
            return MediaDescResult_SameDesc;
        }
    }

    *ret_media_desc__audio = grab (new (std::nothrow) MediaDesc_AAC);
    MediaDesc_AAC * const media_desc__aac = static_cast <MediaDesc_AAC*> ((*ret_media_desc__audio).ptr());
    media_desc__aac->init (44100 /* TODO rate */,
                           2     /* TODO channels */,
                           ConstMemory (aac_sample_descs [sample_description_index - 1].aac_cdata_buf,
                                        aac_sample_descs [sample_description_index - 1].aac_cdata_len));

    return MediaDescResult_Normal;
}

Mp4Reader::MediaDescResult
Mp4Reader::getAvcMediaDescForFrame (Size                   const sample_description_index,
                                    Size                   const last_reported_sample_description_index,
                                    Ref<MediaDesc_Video> * const mt_nonnull ret_media_desc__video)
{
    if (last_reported_sample_description_index == sample_description_index)
        return MediaDescResult_SameDesc;

    if (mt_unlikely (sample_description_index > num_avc_sample_descs)) {
        logE (mp4_reader, _this_func,
              "sample description index ", sample_description_index, " "
              "out of range, ", num_avc_sample_descs, " max");
        return MediaDescResult_NoDesc;
    }

    if (!avc_sample_descs [sample_description_index - 1].avc_cdata_buf) {
        logE (mp4_reader, _this_func, "missing AVC sample description, index ", sample_description_index);
        return MediaDescResult_NoDesc;
    }

    if (last_reported_sample_description_index != 0) {
        assert (avc_sample_descs [last_reported_sample_description_index - 1].avc_cdata_buf);
        if (equal (ConstMemory (avc_sample_descs [last_reported_sample_description_index - 1].avc_cdata_buf,
                                avc_sample_descs [last_reported_sample_description_index - 1].avc_cdata_len),
                   ConstMemory (avc_sample_descs [sample_description_index - 1].avc_cdata_buf,
                                avc_sample_descs [sample_description_index - 1].avc_cdata_len)))
        {
            return MediaDescResult_SameDesc;
        }
    }

    *ret_media_desc__video = grab (new (std::nothrow) MediaDesc_AVC);
    MediaDesc_AVC * const media_desc__avc = static_cast <MediaDesc_AVC*> ((*ret_media_desc__video).ptr());
    media_desc__avc->init (320 /* TODO width */,
                           240 /* TODO height */,
                           25  /* TODO framerate */,
                           ConstMemory (avc_sample_descs [sample_description_index - 1].avc_cdata_buf,
                                        avc_sample_descs [sample_description_index - 1].avc_cdata_len));

    return MediaDescResult_Normal;
}

VodReader::ReadResult
Mp4Reader::readNextAudioFrame (ReadMessage   * const mt_nonnull read_msg,
                               MediaMessage ** const mt_nonnull ret_msg,
                               bool            const stop_time_enabled,
                               Time            const stop_time_microsec,
                               SampleInfo    * const mt_nonnull sample_info)
{
    logD (mp4_reader, _this_func_);

    if (stop_time_enabled) {
        if (sample_info->sample_dts_nanosec / 1000 >= stop_time_microsec)
            return ReadResult::StopTimeReached;
    }

    {
        Ref<MediaDesc_Audio> media_desc__audio;
        MediaDescResult const res = getAacMediaDescForFrame (sample_info->sample_description_index,
                                                             audio_track.last_reported_sample_description_index,
                                                             &media_desc__audio);
        if (res == MediaDescResult_NoDesc) {
            logE (mp4_reader, _this_func, "no AAC media desc");
            return ReadResult::Error;
        }

        audio_track.last_reported_sample_description_index = sample_info->sample_description_index;
        if (res != MediaDescResult_SameDesc) {
            assert (res == MediaDescResult_Normal);

            audio_rate     = media_desc__audio->rate;
            audio_channels = media_desc__audio->channels;

            MediaDescMessage * const msg = &read_msg->media_desc_msg;
            msg->media_desc__audio = std::move (media_desc__audio);
            *ret_msg =msg;

            logD (mp4_reader, _this_func, "returning ReadResult::Normal (AAC desc)");
            return ReadResult::Normal;
        }
    }

    {
        AudioMessage * const msg = &read_msg->audio_msg;

        if (!file.seek (sample_info->sample_offset, SeekOrigin::Beg)) {
            logE (mp4_reader, _this_func, "file.seek() failed: ", exc->toString());
            return ReadResult::Error;
        }

        PagePool::PageListHead page_list;
        page_pool->getPages (&page_list, (Size) sample_info->sample_size);

        PagePool::Page *page = page_list.first;
        Size left = (Size) sample_info->sample_size;
        while (left) {
            Size toread = page_pool->getPageSize();
            if (toread > left)
                toread = left;

            if (!readBytes (Memory (page->getData(), toread))) {
                logE (mp4_reader, _this_func, "readBytes() failed");
                return ReadResult::Error;
            }
            page->data_len = toread;

            page = page->getNextMsgPage();
            left -= toread;
        }

        msg->pts_nanosec = sample_info->sample_cts_nanosec;
        msg->dts_nanosec = sample_info->sample_dts_nanosec;
        msg->page_pool = page_pool;
        msg->page_list = page_list;
        msg->msg_len = sample_info->sample_size;
        msg->msg_offset = 0;
        msg->frame_type = AudioFrameType::RawData;
        msg->codec_id = AudioCodecId::AAC;
        msg->rate     = audio_rate;
        msg->channels = audio_channels;

        logD (mp4_reader, _this_func, "OUTSAMPLE "
              "dt ", sample_info->sample_dts_nanosec, ", "
              "ct ", sample_info->sample_cts_nanosec, ", "
              "of ", sample_info->sample_offset, ", "
              "sz ", sample_info->sample_size);

        /*
        logD (mp4_reader, _this_func, "sample_dts_nanosec ", sample_info->sample_dts_nanosec);
        logD (mp4_reader, _this_func, "sample_cts_nanosec ", sample_info->sample_cts_nanosec);
        logD (mp4_reader, _this_func, "sample_offset      ", sample_info->sample_offset);
        logD (mp4_reader, _this_func, "sample_size        ", sample_info->sample_size);

        logD (mp4_reader, _this_func, "cur_chunk_idx              ", audio_track.cur_chunk_idx);
        logD (mp4_reader, _this_func, "cur_sample_idx             ", audio_track.cur_sample_idx);
        logD (mp4_reader, _this_func, "cur_sample_idx_in_chunk    ", audio_track.cur_sample_idx_in_chunk);
        logD (mp4_reader, _this_func, "cur_sample_offset_in_chunk ", audio_track.cur_sample_offset_in_chunk);
        */

        *ret_msg = msg;
    }

    advanceFrame (&audio_track, sample_info->sample_size);

    logD (mp4_reader, _this_func, "returning ReadResult::Normal");
    return ReadResult::Normal;
}

VodReader::ReadResult
Mp4Reader::readNextVideoFrame (ReadMessage   * const mt_nonnull read_msg,
                               MediaMessage ** const mt_nonnull ret_msg,
                               bool            const stop_time_enabled,
                               Time            const stop_time_microsec,
                               SampleInfo    * const mt_nonnull sample_info)
{
    logD (mp4_reader, _this_func_);

    if (stop_time_enabled) {
        if (sample_info->sample_dts_nanosec / 1000 >= stop_time_microsec)
            return ReadResult::StopTimeReached;
    }

    {
        Ref<MediaDesc_Video> media_desc__video;
        MediaDescResult const res = getAvcMediaDescForFrame (sample_info->sample_description_index,
                                                             video_track.last_reported_sample_description_index,
                                                             &media_desc__video);
        if (res == MediaDescResult_NoDesc) {
            logE (mp4_reader, _this_func, "no AVC media desc");
            return ReadResult::Error;
        }

        video_track.last_reported_sample_description_index = sample_info->sample_description_index;
        if (res != MediaDescResult_SameDesc) {
            assert (res == MediaDescResult_Normal);

            MediaDescMessage * const msg = &read_msg->media_desc_msg;
            msg->media_desc__video = std::move (media_desc__video);
            *ret_msg =msg;

            logD (mp4_reader, _this_func, "returning ReadResult::Normal (AVC desc)");
            return ReadResult::Normal;
        }
    }

    {
        VideoMessage * const msg = &read_msg->video_msg;

        if (!file.seek (sample_info->sample_offset, SeekOrigin::Beg)) {
            logE (mp4_reader, _this_func, "file.seek() failed: ", exc->toString());
            return ReadResult::Error;
        }

        PagePool::PageListHead page_list;
        page_pool->getPages (&page_list, (Size) sample_info->sample_size);

        PagePool::Page *page = page_list.first;
        Size left = (Size) sample_info->sample_size;
        while (left) {
            Size toread = page_pool->getPageSize();
            if (toread > left)
                toread = left;

            if (!readBytes (Memory (page->getData(), toread))) {
                logE (mp4_reader, _this_func, "readBytes() failed");
                return ReadResult::Error;
            }
            page->data_len = toread;

            page = page->getNextMsgPage();
            left -= toread;
        }

        msg->pts_nanosec = sample_info->sample_cts_nanosec;
        msg->dts_nanosec = sample_info->sample_dts_nanosec;
        msg->page_pool = page_pool;
        msg->page_list = page_list;
        msg->msg_len = sample_info->sample_size;
        msg->msg_offset = 0;
        msg->frame_type = VideoFrameType::KeyFrame; // TODO Set proper VideoFrameType:
                                                    //        a) parse stss for sync samples;
                                                    //        b) look for type 5 NAL units in page_list.
        msg->codec_id = VideoCodecId::AVC;

        logD (mp4_reader, _this_func, "OUTSAMPLE "
              "dt ", sample_info->sample_dts_nanosec, ", "
              "ct ", sample_info->sample_cts_nanosec, ", "
              "of ", sample_info->sample_offset, ", "
              "sz ", sample_info->sample_size);

        /*
        logD (mp4_reader, _this_func, "sample_dts_nanosec ", sample_info->sample_dts_nanosec);
        logD (mp4_reader, _this_func, "sample_cts_nanosec ", sample_info->sample_cts_nanosec);
        logD (mp4_reader, _this_func, "sample_offset      ", sample_info->sample_offset);
        logD (mp4_reader, _this_func, "sample_size        ", sample_info->sample_size);

        logD (mp4_reader, _this_func, "cur_chunk_idx              ", video_track.cur_chunk_idx);
        logD (mp4_reader, _this_func, "cur_sample_idx             ", video_track.cur_sample_idx);
        logD (mp4_reader, _this_func, "cur_sample_idx_in_chunk    ", video_track.cur_sample_idx_in_chunk);
        logD (mp4_reader, _this_func, "cur_sample_offset_in_chunk ", video_track.cur_sample_offset_in_chunk);
        */

        *ret_msg = msg;
    }

    advanceFrame (&video_track, sample_info->sample_size);

    logD (mp4_reader, _this_func, "returning ReadResult::Normal");
    return ReadResult::Normal;
}

VodReader::SeekResult
Mp4Reader::seek (Time const seek_pos_microsec)
{
    logD (mp4_reader, _this_func_);

    if (error_state)
        return SeekResult::Error;

    if (!header_parsed) {
        header_parsed = true;
        if (!parseHeader ()) {
            error_state = true;
            return SeekResult::Error;
        }
    }

    if (got_video_track) {
        ReadResult const res = seekTrack (&video_track, seek_pos_microsec);
        switch ((ReadResult::Value) res) {
            case ReadResult::Normal: {
                // No-op
            } break;
            case ReadResult::Error: {
                logE (mp4_reader, _this_func, "seekTrack() failed (video)");
                error_state = true;
                return SeekResult::Error;
            } break;
            case ReadResult::NoMoreFrames: {
                return SeekResult::SeekPastEnd;
            } break;
            case ReadResult::StopTimeReached: {
                unreachable ();
            } break;
        }

        // TODO if SeekPastEnd, then try audio only
    }

    if (got_audio_track) {
        ReadResult const res = seekTrack (&audio_track, seek_pos_microsec);
        switch ((ReadResult::Value) res) {
            case ReadResult::Normal: {
              // No-op
            } break;
            case ReadResult::Error: {
                logE (mp4_reader, _this_func, "seekTrack() failed (audio)");
                error_state = true;
                return SeekResult::Error;
            } break;
            case ReadResult::NoMoreFrames: {
                return SeekResult::SeekPastEnd;
            } break;
            case ReadResult::StopTimeReached: {
                unreachable ();
            } break;
        }

        // TODO if SeekPastEnd, ten try video only
    }

    return SeekResult::Normal;
}

VodReader::ReadResult
Mp4Reader::seekTrack (Track * const mt_nonnull track,
                      Time    const seek_pos_microsec)
{
    logD (mp4_reader, _this_func, "track 0x", fmt_hex, (UintPtr) track, fmt_def, ", "
          "seek_pos_microsec ", seek_pos_microsec);

    track->track_end_reached = false;

    // stts seek
    track->cur_sample_idx = 0;
    {
        track->stts.pos = 0;
        track->stts.page_pos = 0;
        track->stts.page->data_len = 0;

        track->cur_stts_sample_count =0 ;
        track->cur_stts_sample_delta_nanosec = 0;
        track->cur_sample_stts_idx = 0;

        track->cur_sample_dts_nanosec = 0;

        Time const seek_pos_nanosec = seek_pos_microsec * 1000;
        for (;;) {
            logD (mp4_reader, _this_func, "cur_sample_dts_nanosec ", track->cur_sample_dts_nanosec);

            if (track->cur_sample_dts_nanosec >= seek_pos_nanosec)
                break;

            {
                ReadResult const res = readSttsIfNeeded (track);
                if (res == ReadResult::NoMoreFrames) {
                    track->track_end_reached = true;
                    return ReadResult::NoMoreFrames;
                } else
                if (res == ReadResult::Error) {
                    logE (mp4_reader, _this_func, "readSttsIfNeeded() failed");
                    return ReadResult::Error;
                } else
                    assert (res == ReadResult::Normal);
            }

            // TODO jump by cur_stts_sample_count - cur_sample_stts_idx

            ++track->cur_sample_idx;
            ++track->cur_sample_stts_idx;
            track->cur_sample_dts_nanosec += track->cur_stts_sample_delta_nanosec;
        }
    }

    // ctts seek
    if (!track->ctts.box.isNull()) {
        track->ctts.pos = 0;
        track->ctts.page_pos = 0;
        track->ctts.page->data_len = 0;

        track->cur_ctts_sample_count = 0;
        track->cur_ctts_sample_offset_nanosec = 0;
        track->cur_sample_ctts_idx = 0;

        Uint64 sample_idx = 0;
        for (;;) {
            if (sample_idx >= track->cur_sample_idx)
                break;

            {
                ReadResult const res = readCttsIfNeeded (track);
                if (res == ReadResult::NoMoreFrames) {
                    track->track_end_reached = true;
                    return ReadResult::NoMoreFrames;
                } else
                if (res == ReadResult::Error) {
                    logE (mp4_reader, _this_func, "readCttsIfNeeded() failed");
                    return ReadResult::Error;
                } else
                    assert (res == ReadResult::Normal);
            }

            // TODO jump by cur_ctts_sample_count - cur_sample_ctts_idx

            ++track->cur_sample_ctts_idx;
            ++sample_idx;
        }
    }

    // stsc seek
    {
        track->stsc.pos = 0;
        track->stsc.page_pos = 0;
        track->stsc.page->data_len = 0;

        track->cur_chunk_idx = 0;
        track->cur_sample_idx_in_chunk = 0;
        track->cur_sample_offset_in_chunk = 0;

        track->next_stsc_chunk_idx = 0;
        track->got_next_stsc_chunk_idx = true;

        Uint64 sample_idx = 0;
        for (;;) {
            // ctts.samples_per_chunk may be 0, hence we have to read stsc
            // to get cur_chunk_idx.
            {
                ReadResult const res = readStscIfNeeded (track);
                if (res == ReadResult::NoMoreFrames) {
                    track->track_end_reached = true;
                    return ReadResult::NoMoreFrames;
                } else
                if (res == ReadResult::Error) {
                    logE (mp4_reader, _this_func, "readStscIfNeeded() failed");
                    return ReadResult::Error;
                } else
                    assert (res == ReadResult::Normal);
            }

            if (sample_idx >= track->cur_sample_idx)
                break;

            // TODO jump by ...got_next_stsc_chunk...

            ++track->cur_sample_idx_in_chunk;
            ++sample_idx;
        }
    }

    // stco seek
    if (track->cur_stco_chunk_idx != track->cur_chunk_idx) {
        if (track->cur_chunk_idx > track->stco.num_entries) {
            logD (mp4_reader, _this_func,
                  "out of stco entries: "
                  "cur_chunk_idx ", track->cur_chunk_idx, ", "
                  "stco.num_entries ", track->stco.num_entries);
            track->track_end_reached = true;
            return ReadResult::NoMoreFrames;
        }

        track->stco.pos = 0;
        {
            Uint64 const fixed_idx = (track->cur_chunk_idx > 0 ? track->cur_chunk_idx - 1 : 0);
            track->stco.page_pos = fixed_idx * MOMENT__STCO_ENTRY_SIZE;
        }
        track->stco.page->data_len = 0;

        // Unnecessary
        // readStcoIfNeeded (track);
    }

    // stsz seek
    if (track->stsz_sample_size == 0) {
        assert (track->cur_sample_idx_in_chunk <= track->cur_sample_idx);

        if (track->cur_sample_idx >= track->stsz.num_entries) {
            logD (mp4_reader, _this_func,
                  "out of stsz entries: "
                  "sample_idx ", track->cur_sample_idx, ", "
                  "stsz.num_entries ", track->stsz.num_entries);
            track->track_end_reached = true;
            return ReadResult::NoMoreFrames;
        }

        Uint64 sample_idx = track->cur_sample_idx - track->cur_sample_idx_in_chunk;

        track->stsz.pos = 0;
        track->stsz.page_pos = sample_idx * MOMENT__STSZ_ENTRY_SIZE;
        track->stsz.page->data_len = 0;

        track->cur_sample_offset_in_chunk = 0;
        if (track->cur_sample_idx_in_chunk > 0) {
            do {
                {
                    ReadResult const res = readTableIfNeeded (&track->stsz,
                                                              MOMENT__STSZ_ENTRY_SIZE,
                                                              MOMENT__STSZ_DATA_OFFSET);
                    if (res == ReadResult::NoMoreFrames) {
                        track->track_end_reached = true;
                        return ReadResult::NoMoreFrames;
                    } else
                    if (res == ReadResult::Error) {
                        logE (mp4_reader, _this_func, "readTableIfNeeded() failed (stsz)");
                        return ReadResult::Error;
                    } else
                        assert (res == ReadResult::Normal);
                }

                Uint32 const sample_size = readBe32 (track->stsz.page->getData() + track->stsz.pos);
                track->stsz.pos += MOMENT__STSZ_ENTRY_SIZE;

                track->cur_sample_offset_in_chunk += sample_size;

                ++sample_idx;
            } while (sample_idx < track->cur_sample_idx);
        }
    } else {
        track->cur_sample_offset_in_chunk = track->cur_sample_idx_in_chunk * track->stsz_sample_size;
    }

    return ReadResult::Normal;
}

Result
Mp4Reader::getSourceInfo (VodSourceInfo * const mt_nonnull source_info)
{
    logD (mp4_reader, _this_func_);

    source_info->media_desc__audio = NULL;
    source_info->media_desc__video = NULL;

    if (mt_unlikely (error_state)) {
        logD (mp4_reader, _this_func, "error_state");
        return Result::Failure;
    }

    if (mt_unlikely (!header_parsed)) {
        header_parsed = true;
        if (!parseHeader ()) {
            error_state = true;
            logD (mp4_reader, _this_func, "parseHeader() failed");
            return Result::Failure;
        }
    }

    logD (mp4_reader, _this_func,
          "got_audio_track ", got_audio_track, ", "
          "got_video_track ", got_video_track);

    SampleInfo audio_sample_info;
    SampleInfo video_sample_info;

    ReadResult audio_res = ReadResult::NoMoreFrames;
    if (   got_audio_track
        && !audio_track.track_end_reached)
    {
        audio_res = peekNextFrame (&audio_track, &audio_sample_info);
        assert (audio_res != ReadResult::StopTimeReached);
    }

    ReadResult video_res = ReadResult::NoMoreFrames;
    if (   got_video_track
        && !video_track.track_end_reached)
    {
        video_res = peekNextFrame (&video_track, &video_sample_info);
        assert (audio_res != ReadResult::StopTimeReached);
    }

    if (audio_res == ReadResult::Normal) {
        MediaDescResult const res = getAacMediaDescForFrame (audio_sample_info.sample_description_index,
                                                             0 /* last_reported_sample_description_index */,
                                                             &source_info->media_desc__audio);
        if (res != MediaDescResult_Normal) {
            logE (mp4_reader, _this_func, "no AAC media desc");
            source_info->media_desc__audio = NULL;
        }
    }

    if (video_res == ReadResult::Normal) {
        MediaDescResult const res = getAvcMediaDescForFrame (video_sample_info.sample_description_index,
                                                             0 /* last_reported_sample_description_index */,
                                                             &source_info->media_desc__video);
        if (res != MediaDescResult_Normal) {
            logE (mp4_reader, _this_func, "no AVC media desc");
            source_info->media_desc__video = NULL;
        }
    }

    return Result::Success;
}

Result
Mp4Reader::open (ConstMemory const filename)
{
    logD (mp4_reader, _this_func_);

    if (!file.open (filename, 0 /* open_flags */, FileAccessMode::ReadOnly)) {
        logE (mp4_reader, _this_func, "could not open file \"", filename, "\": ", exc->toString());
        return Result::Failure;
    }

    this->filename = newString (filename);

    return Result::Success;
}

void
Mp4Reader::init (PagePool * const mt_nonnull page_pool)
{
    this->page_pool = page_pool;
}

Mp4Reader::Mp4Reader ()
    : got_audio_track      (false),
      got_video_track      (false),
      avc_sample_descs     (NULL),
      aac_sample_descs     (NULL),
      num_avc_sample_descs (0),
      num_aac_sample_descs (0),
      file                 (NULL /* embed_container */),
      header_parsed        (false),
      error_state          (false),
      audio_rate           (44100),
      audio_channels       (2)
{
    logD (mp4_reader, _this_func_);
}

Mp4Reader::~Mp4Reader ()
{
    logD (mp4_reader, _this_func_);

    delete[] avc_sample_descs;
    delete[] aac_sample_descs;
}

}

