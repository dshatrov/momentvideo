/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents,
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MCONFIG__VARLIST__H__
#define MCONFIG__VARLIST__H__


#include <libmary/libmary.h>

#include <mconfig/config.h>


namespace MConfig {

using namespace M;

class Varlist : public Object
{
public:
    class Var : public IntrusiveListElement<>
    {
        friend class Varlist;

    private:
        Byte *name_buf;
        Size  name_len;

        Byte *value_buf;
        Size  value_len;

    public:
        ConstMemory getName  () const { return ConstMemory (name_buf,  name_len);  }
        ConstMemory getValue () const { return ConstMemory (value_buf, value_len); }

        ~Var ()
        {
            delete[] name_buf;
            delete[] value_buf;
        }
    };

    typedef IntrusiveList<Var> VarList;

    class Section : public IntrusiveListElement<>
    {
        friend class Varlist;

    private:
        Byte *name_buf;
        Size  name_len;

        bool enabled;

    public:
        ConstMemory getName () const { return ConstMemory (name_buf, name_len); }

        bool getEnabled () const { return enabled; }

        ~Section () { delete[] name_buf; }
    };

    typedef IntrusiveList<Section> SectionList;

    VarList var_list;
    SectionList section_list;

    void addEntry (ConstMemory name,
                   ConstMemory value,
                   bool        with_value,
                   bool        enable_section,
                   bool        disable_section);

     Varlist (EmbedContainer * const embed_container)
         : Object (embed_container)
     {}

    ~Varlist ();
};

void parseVarlistSection (MConfig::Section * mt_nonnull section,
                          MConfig::Varlist * mt_nonnull varlist);

}


#endif /* MCONFIG__VARLIST__H__ */

