/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__PLAYLIST__H__
#define MOMENT__PLAYLIST__H__


#include <libmary/types.h>
#ifdef MOMENT_LIBXML2
  #include <libxml/parser.h>
#endif
#include <libmary/libmary.h>

#include <moment/channel_options.h>


namespace Moment {

using namespace M;

mt_unsafe class Playlist
{
// TODO private
  public:
    class Item : public StReferenced,
                 public IntrusiveListElement<>,
                 public HashEntry<>
    {
      public:
        // Valid if 'start_immediate' is false.
        Time start_time;
        bool start_immediate;

        // Valid if 'got_end_time' is true.
        Time end_time;
        bool got_end_time;

        // Valid if both 'duration_full' and 'duration_default' are false.
        Time duration;
        bool duration_full;
        bool duration_default;

        Time seek;
        Ref<PlaybackItem> playback_item;

        StRef<String> id;

        void reset ()
        {
            start_time = 0;
            start_immediate = true;

            end_time = 0;
            got_end_time = false;

            duration = 0;
            duration_full = false;
            duration_default = true;

            seek = 0;
        }

        Item ()
        {
            reset ();
        }
    };

    typedef IntrusiveList<Item> ItemList;

    typedef Hash< Item,
                  Memory,
                  MemberExtractor< Item,
                                   StRef<String>,
                                   &Item::id,
                                   Memory,
                                   AccessorExtractor< String,
                                                      Memory,
                                                      &String::mem > >,
                  MemoryComparator<> >
            ItemHash;

    ItemList item_list;
    ItemHash item_hash;

    // If non-null, then the directory should be re-read for every getNextItem() call.
    // ...possible alternative - re-read after all known files have been played.
    StRef<String> from_dir;
    bool from_dir_is_relative;
    Ref<PlaybackItem> from_dir_default_playback_item;

  #ifdef MOMENT_LIBXML2
    void doParsePlaylist_XML (xmlDocPtr     doc,
                              PlaybackItem * mt_nonnull default_playback_item);

    static void parseItem_XML (xmlDocPtr     doc,
                               xmlNodePtr    media_node,
                               PlaybackItem * mt_nonnull default_playback_item,
                               Item         * mt_nonnull item);

    static void parseItemAttributes_XML (xmlNodePtr  node,
                                         Item       * mt_nonnull item);
  #endif

    Result parseItem_Conf (MConfig::Section * mt_nonnull item_section,
                           PlaybackItem     * mt_nonnull default_playback_item,
                           Item             * mt_nonnull item);

    mt_throws Result doReadDirectory (ConstMemory   from_dir,
                                      bool          relative,
                                      PlaybackItem * mt_nonnull default_playback_item);

  public:
    // @cur_time - Current unixtime.
    Item* getNextItem (Item  *prv_item,
                       Time   cur_time,
                       Int64  time_offset,
                       Time  * mt_nonnull ret_start_rel,
                       Time  * mt_nonnull ret_seek,
                       Time  * mt_nonnull ret_duration,
                       bool  * mt_nonnull ret_duration_full);

    Item* getItemById (ConstMemory id);

    Item* getNthItem (Count idx);

    void clear ();

    void setSingleItem (PlaybackItem * mt_nonnull playback_item);

    void setSingleChannelRecorder (ConstMemory channel_name);

    mt_throws Result parsePlaylistFile_XML (ConstMemory    filename,
                                            PlaybackItem  * mt_nonnull default_playback_item,
                                            StRef<String> *ret_err_msg);

    mt_throws Result parsePlaylistFile_Conf (ConstMemory    filename,
                                             PlaybackItem  * mt_nonnull default_playback_item);

    Result parsePlaylist_ConfSection (MConfig::Section * mt_nonnull section,
                                      PlaybackItem     * mt_nonnull default_playback_item);

#if 0
// Unused
    mt_throws Result parsePlaylistMem_XML (ConstMemory    mem,
                                           PlaybackItem  * mt_nonnull default_playback_item,
                                           StRef<String> *ret_err_msg);

    mt_throws Result parsePlaylistMem_Conf (ConstMemory    mem,
                                            PlaybackItem  * mt_nonnull default_playback_item,
                                            StRef<String> *ret_err_msg);
#endif

    mt_throws Result readDirectory (ConstMemory   from_dir,
                                    bool          re_read,
                                    PlaybackItem * mt_nonnull default_playback_item);

    void dump ();

    ~Playlist ();
};

}


#endif /* MOMENT__PLAYLIST__H__ */

