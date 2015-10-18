/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/h264_parser.h>


#warning TODO limit stored access unit size


namespace Moment {

static LogGroup libMary_logGroup_h264parse ("h264parse", LogLevel::I);

bool
H264Parser::getAvcDecoderConfigurationRecord (PagePool::PageListHead * const mt_nonnull ret_page_list,
                                              Size                   * const mt_nonnull ret_msg_offs,
                                              Size                   * const mt_nonnull ret_msg_len)
{
    logS (h264parse, _this_func_);

    ret_page_list->reset ();
    *ret_msg_offs = 0;
    *ret_msg_len  = 0;

    unsigned first_valid_sps_idx = 0;
    bool got_valid_sps = false;

    unsigned num_sps = 0;
    for (unsigned i = 0; i < MOMENT__H264_PARSER__MAX_SPS_COUNT; ++i) {
        if (sps [i].mem.len()) {
            if (!got_valid_sps) {
                first_valid_sps_idx = i;
                got_valid_sps = true;
            }

            ++num_sps;
        }
    }
    if (!num_sps) {
        logD (h264parse, _this_func, "no SPS");
        return false;
    }
    assert (got_valid_sps);

    unsigned num_pps = 0;
    for (unsigned i = 0; i < MOMENT__H264_PARSER__MAX_PPS_COUNT; ++i) {
        if (pps [i].mem.len())
            ++num_pps;
    }
    if (!num_pps) {
        logD (h264parse, _this_func, "no PPS");
        return false;
    }

    {
        Byte header [] = {
            1,
            // TODO You may want to derive profile parameters in a more intelligent
            //      way, or take profile-level-id from RTSP SDP.
            sps [first_valid_sps_idx].mem.buf() [1],
            sps [first_valid_sps_idx].mem.buf() [2],
            sps [first_valid_sps_idx].mem.buf() [3],
            0xff,
            Byte (0xe0 | (num_sps & 0x1f))
        };
        page_pool->getFillPages (ret_page_list, ConstMemory::forObject (header));
        *ret_msg_len += sizeof (header);
    }

    for (unsigned i = 0; i < MOMENT__H264_PARSER__MAX_SPS_COUNT; ++i) {
        if (sps [i].mem.len()) {
            logD_ (_this_func, "sps #", i, ": ", makeString (fmt_hex_mem, sps [i].mem));

            {
                Byte sps_size [] = {
                    (Byte) ((sps [i].mem.len() >> 8) & 0xff),
                    (Byte) ((sps [i].mem.len() >> 0) & 0xff)
                };
                page_pool->getFillPages (ret_page_list, ConstMemory::forObject (sps_size));
                *ret_msg_len += sizeof (sps_size);
            }
            page_pool->getFillPages (ret_page_list, sps [i].mem);
            *ret_msg_len += sps [i].mem.len();
        }
    }

    {
        Byte pps_header [] = { (Byte) num_pps };
        page_pool->getFillPages (ret_page_list, ConstMemory::forObject (pps_header));
        *ret_msg_len += sizeof (pps_header);
    }

    for (unsigned i = 0; i < MOMENT__H264_PARSER__MAX_PPS_COUNT; ++i) {
        if (pps [i].mem.len()) {
            logD_ (_this_func, "pps #", i, ": ", makeString (fmt_hex_mem, pps [i].mem));

            {
                Byte pps_size [] = {
                    (Byte) ((pps [i].mem.len() >> 8) & 0xff),
                    (Byte) ((pps [i].mem.len() >> 0) & 0xff)
                };
                page_pool->getFillPages (ret_page_list, ConstMemory::forObject (pps_size));
                *ret_msg_len += sizeof (pps_size);
            }
            page_pool->getFillPages (ret_page_list, pps [i].mem);
            *ret_msg_len += pps [i].mem.len();
        }
    }

    logS (h264parse, _this_func, "msg_len ", *ret_msg_len);
    return true;
}

void
H264Parser::reportParameterSets ()
{
    if (!parameter_sets_changed) {
        return;
    }
    parameter_sets_changed = false;

    if (cb->parameterSets) {
        PagePool::PageListHead page_list;
        Size msg_offs;
        Size msg_len;
        if (!getAvcDecoderConfigurationRecord (&page_list, &msg_offs, &msg_len))
            return;

        cb->parameterSets (&page_list,
                           msg_offs,
                           msg_len,
                           au_pts,
                           au_dts,
                           cb_data);

        page_pool->msgUnref (page_list.first, msg_len, msg_offs);
    }
}

void
H264Parser::doCompleteAccessUnit ()
{
    logS (h264parse, _this_func_);

    reportParameterSets ();

    if (cb->accessUnit) {
        cb->accessUnit (&au_page_list,
                        0 /* msg_offs */,
                        au_len,
                        au_is_keyframe,
                        au_pts,
                        au_dts,
                        cb_data);
    }
}

void
H264Parser::maybeCompleteAccessUnit (Byte const next_nal_type)
{
    if (got_au_begin) {
        doCompleteAccessUnit ();
        resetAccessUnit ();
    } else
    if (next_nal_type == 9 /* AU delimiter */) {
        if (au_len > 0) {
            if (au_delimiter_status != AuDelimiterStatus_GotAuDelimiterOnly) {
                doCompleteAccessUnit ();
            } else {
                logS (h264parse, _this_func, "discarding lone AU delimiter");
            }
        }

        resetAccessUnit ();
    }
}

void
H264Parser::resetAccessUnit ()
{
    logS (h264parse, _this_func_);

    page_pool->msgUnref (au_page_list.first);
    au_page_list.reset ();

    au_len         = 0;
    au_is_keyframe = false;
    got_au_begin   = false;

    au_delimiter_status = AuDelimiterStatus_NoAuDelimiter;
}

void
H264Parser::parseSpsNal (PagePool::Page * const page,
                         Size             const msg_offs,
                         Size             const msg_len)
{
    if (mt_unlikely (msg_len < 4)) {
        logS (h264parse, _this_func, "short SPS NAL, msg_len ", msg_len);
        return;
    }

    unsigned seq_parameter_set_id;
    {
        LinearMsgBitReader bit_reader (page, msg_len - 4, msg_offs + 4);
        Uint64 number;
        if (!readExpGolombUint (&bit_reader, &number)) {
            logE (h264parse, _this_func, "could not read seq_parameter_set_id");
            return;
        }

        if (number >= MOMENT__H264_PARSER__MAX_SPS_COUNT) {
            logE (h264parse, _this_func, "seq_parameter_set_id is too large: ", number);
            return;
        }

        seq_parameter_set_id = (unsigned) number;
    }

    if (PagePool::pagesEqualToMemory (page, msg_offs, msg_len, sps [seq_parameter_set_id].mem)) {
        logS (h264parse, _this_func, "same SPS [", seq_parameter_set_id, "]");
        return;
    }
    logS (h264parse, _this_func, "new SPS [", seq_parameter_set_id, "], old len ", sps [seq_parameter_set_id].mem.len());
    if (logLevelOn (h264parse, LogLevel::S))
        PagePool::logPagesMsg (LogLevel::S, page, msg_offs, msg_len, _this_func, "SPS: ");

    sps [seq_parameter_set_id].allocate (msg_len);
    PagePool::copyMsgToMemory (page, msg_offs, msg_len, sps [seq_parameter_set_id].mem.buf());
    parameter_sets_changed = true;
}

void
H264Parser::parsePpsNal (PagePool::Page * const page,
                         Size             const msg_offs,
                         Size             const msg_len)
{
    if (mt_unlikely (msg_len < 1)) {
        logS (h264parse, _this_func, "short PPS NAL, msg_len ", msg_len);
        return;
    }

    unsigned pic_parameter_set_id;
    {
        LinearMsgBitReader bit_reader (page, msg_len - 1, msg_offs + 1);
        Uint64 number;
        if (!readExpGolombUint (&bit_reader, &number)) {
            logE (h264parse, _this_func, "could not read pic_parameter_set_id");
            return;
        }

        if (number >= MOMENT__H264_PARSER__MAX_PPS_COUNT) {
            logE (h264parse, _this_func, "pic_parameter_set_id is too large: ", number);
            return;
        }

        pic_parameter_set_id = (unsigned) number;
    }

    if (PagePool::pagesEqualToMemory (page, msg_offs, msg_len, pps [pic_parameter_set_id].mem)) {
        logS (h264parse, _this_func, "same PPS [", pic_parameter_set_id, "]");
        return;
    }
    logS (h264parse, _this_func, "new PPS [", pic_parameter_set_id, "], old len ", pps [pic_parameter_set_id].mem.len());
    if (logLevelOn (h264parse, LogLevel::S))
        PagePool::logPagesMsg (LogLevel::S, page, msg_offs, msg_len, _this_func, "PPS: ");

    pps [pic_parameter_set_id].allocate (msg_len);
    PagePool::copyMsgToMemory (page, msg_offs, msg_len, pps [pic_parameter_set_id].mem.buf());
    parameter_sets_changed = true;
}

void
H264Parser::parseOutOfBandParametersNal (ConstMemory const nal_mem)
{
    if (nal_mem.len() < 1)
        return;

    Byte const nal_type = nal_mem.buf() [0] & 0x1f;
    if (nal_type == 7) {
        if (nal_mem.buf() [nal_mem.len() - 1] == 0) {
            logE (h264parse, _this_func, "bad out-of-band SPS");
            return;
        }

        PagePool::PageListHead nal_pages;
        page_pool->getFillPages (&nal_pages, nal_mem);
        parseSpsNal (nal_pages.first, 0 /* msg_offs */, nal_mem.len());
        page_pool->msgUnref (nal_pages.first, nal_mem.len(), 0 /* msg_offs */);
    } else
    if (nal_type == 8) {
        // Some buggy IP cameras return bad PPS in sprop-parameter-sets.
        if (nal_mem.buf() [nal_mem.len() - 1] == 0) {
            logE (h264parse, _this_func, "bad out-of-band PPS");
            return;
        }

        PagePool::PageListHead nal_pages;
        page_pool->getFillPages (&nal_pages, nal_mem);
        parsePpsNal (nal_pages.first, 0 /* msg_offs */, nal_mem.len());
        page_pool->msgUnref (nal_pages.first, nal_mem.len(), 0 /* msg_offs */);
    }
}

void
H264Parser::parseNal (PagePool::Page * const page,
                      Size             const msg_offs,
                      Size             const msg_len,
                      Time             const pts,
                      Time             const dts,
                      bool             const last_in_access_unit)
{
    // Here we reconstruct access units from individual NAL units.

    Byte const nal_type = PagePool::getMsgByte (page, msg_offs + 0) & 0x1f;

    logS (h264parse, _this_func,
          "nal_type ", (Uint32) nal_type, ", "
          "msg_offs ", msg_offs, ", "
          "msg_len ", msg_len, ", "
          "pts ", pts);

    if (nal_type == 7)
        parseSpsNal (page, msg_offs, msg_len);
    else
    if (nal_type == 8)
        parsePpsNal (page, msg_offs, msg_len);

    // If both 'end' and 'got_au_begin' are true, then this NAL does not belong
    // to the access unit accumulated in 'au_page_list', and we finalize
    // the previous access unit.
    //
    // If 'end' is true and 'got_au_begin' is false, then this NAL still belongs
    // to the access unit accumulated in 'au_page_list'.
    //
    bool end = false;
    if (   pts != au_pts
        || dts != au_dts)
    {
        end  = true;
    }

    // If 'begin' is true, then we have an AU-statring VCL NAL unit for the current access unit.
    bool begin = false;
    if (nal_type == 1 || nal_type == 2 || nal_type == 5) {
        begin = true;

        if (msg_len < 6) {
            logW (h264parse, _this_func, "short slice NAL unit");
            return;
        }

        // We interpret first_mb_in_slice == 0 as picture start.
        // first_mb_in_slice is Exp-Golomb coded. 0 is coded as a single "1" bit.
        if (PagePool::getMsgByte (page, msg_offs + 1) & 0x80)
            end = true;
    } else
    if (nal_type >= 6 && nal_type <= 9) {
      // SEI, SPS, PPS, AU delimiter
        end = true;
    }

    if (end)
        maybeCompleteAccessUnit (nal_type);

    if (nal_type == 9) {
        assert (au_len == 0); // resetAccessUnit() should've been just called above
        au_delimiter_status = AuDelimiterStatus_GotAuDelimiterOnly;
    } else {
        if (au_delimiter_status == AuDelimiterStatus_NoAuDelimiter) {
            Byte const au_delimiter [] = { 0x00, 0x00, 0x00 , 0x02, 0x09, 0xf0 };
            page_pool->getFillPages (&au_page_list, ConstMemory::forObject (au_delimiter));
            au_len += sizeof (au_delimiter);

            au_delimiter_status = AuDelimiterStatus_GotAuDelimiter;
        }
    }

    {
        Byte nal_len [4];
        writeBe32 (nal_len, msg_len);
        page_pool->getFillPages (&au_page_list, ConstMemory::forObject (nal_len));
    }
    page_pool->getFillPagesFromPages (&au_page_list, page, msg_offs, msg_len);
    au_len += msg_len + 4;

    logS (h264parse, _this_func, "au_len: ", au_len);

//#warning TODO 2. Collect and signal codec data;
//#warning TODO 3. Insert codec data in front of IDR frames.

    au_pts = pts;
    au_dts = dts;

    au_is_keyframe |= (nal_type == 5);
    got_au_begin |= begin;

    if (last_in_access_unit)
        maybeCompleteAccessUnit (/*next_nal_type=*/ 9 /* simulating trailing AU delimiter */);
}

void
H264Parser::parseAvcNals (PagePool::Page *page,
                          Size            msg_offs,
                          Size            msg_len,
                          Time             const pts,
                          Time             const dts)
{
    logS (h264parse, _this_func, "msg_len ", msg_len, ", pts ", pts);

    for (;;) {
        if (msg_len == 0)
            break;

        if (msg_len < 4) {
            logD_ (_this_func, "WARNING: discarding ", msg_len, " bytes");
            break;
        }

        while (page->data_len <= msg_offs) {
            msg_offs -= page->data_len;
            page = page->getNextMsgPage();
        }

        PagePool::PageListArray pl_arr (page, msg_offs, msg_len);

        Byte nal_len_buf [4];
        pl_arr.get (/*offset=*/ 0, Memory::forObject (nal_len_buf));
        Uint32 const nal_len = readBe32 (nal_len_buf);

        if (msg_offs + 4 < msg_offs) {
            logD_ (_this_func, "WARNING: integer overflow, msg_offs ", msg_offs);
            break;
        }
        msg_offs += 4;

        while (page->data_len <= msg_offs) {
            msg_offs -= page->data_len;
            page = page->getNextMsgPage();
        }

        msg_len -= 4;
        if (nal_len > msg_len) {
            logD_ (_this_func, "WARNING: invalid nal_len ", nal_len, ", msg_len ", msg_len);
            break;
        }
        msg_len -= nal_len;

        parseNal (page, msg_offs, nal_len, pts, dts, /*last_in_access_unit=*/ false);

        if (msg_offs + nal_len < msg_offs) {
            logD_ (_this_func, "WARNING: integer overflow, msg_offs ", msg_offs, ", nal_len ", nal_len);
            break;
        }
        msg_offs += nal_len;
    }
}

void
H264Parser::parseByteStreamNals (PagePool::Page *page,
                                 Size            msg_offs,
                                 Size            msg_len,
                                 Time             const pts,
                                 Time             const dts)
{
    logS (h264parse, _this_func, "msg_len ", msg_len, ", pts ", pts);

    PagePool::Page *first_nal_page = NULL;
    Size nal_offs = 0;
    Size nal_len  = 0;

    Size num_zeroes = 0;

    Byte *buf = page->getData();
    for (;;) {
        if (!msg_len)
            break;

        while (page->data_len <= msg_offs) {
            msg_offs -= page->data_len;
            page = page->getNextMsgPage();
            buf  = page->getData();
        }

        Byte const byte = buf [msg_offs];
        if (byte == 0) {
            ++nal_len;
            ++num_zeroes;
        } else {
            if (byte == 1 && num_zeroes >= 2) {
                if (first_nal_page) {
                    parseNal (first_nal_page,
                              nal_offs,
                              nal_len - num_zeroes,
                              pts,
                              dts,
                              false /* last_in_access_unit */);
                }

                first_nal_page = page;
                nal_offs = msg_offs + 1;
                while (first_nal_page && first_nal_page->data_len <= nal_offs) {
                    nal_offs -= first_nal_page->data_len;
                    first_nal_page = first_nal_page->getNextMsgPage();
                }

                nal_len = 0;
            } else {
                ++nal_len;
            }

            num_zeroes = 0;
        }

        ++msg_offs;
        --msg_len;
    }

    if (first_nal_page) {
        parseNal (first_nal_page,
                  nal_offs,
                  nal_len - num_zeroes,
                  pts,
                  dts,
                  false /* last_in_access_unit */);
    }
}

void
H264Parser::forceAccessUnitEnd ()
{
    maybeCompleteAccessUnit (/*next_nal_type=*/ 9 /* simulating AU delimiter */);
}

mt_const void
H264Parser::init (PagePool        * const mt_nonnull page_pool,
                  Callbacks const * const mt_nonnull cb,
                  void            * const cb_data)
{
    this->page_pool = page_pool;

    this->cb      = cb;
    this->cb_data = cb_data;
}

}

