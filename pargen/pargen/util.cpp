/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <pargen/util.h>


using namespace M;

namespace Pargen {

static char
capitalizeSymbol (char s)
{
    if (s >= 'a' && s <= 'z') {
	if ('A' > 'a')
	    s += 'A' - 'a';
	else
	    s -= 'a' - 'A';
    }

    return s;
}

static char
lowercaseSymbol (char s)
{
    if (s >= 'A' && s <= 'Z') {
	if ('A' > 'a')
	    s -= 'A' - 'a';
	else
	    s += 'a' - 'A';
    }

    return s;
}

StRef<String>
capitalizeName (ConstMemory const name,
		bool        const keep_underscore)
{
    if (name.len() == 0)
	return st_grab (new (std::nothrow) String);

    assert (name.mem());

    Size str_len = 0;
    for (Size i = 0, i_end = name.len(); i < i_end; i++) {
	if (name.mem() [i] == ' ' ||
	    name.mem() [i] == '-')
	{
	    continue;
	}

	if (name.mem() [i] == '_' && !keep_underscore)
	    continue;

	++str_len;
    }

    if (str_len == 0)
	return st_grab (new (std::nothrow) String);

    StRef<String> const str = st_grab (new (std::nothrow) String);
    str->allocate (str_len);

    bool capital = true;
    for (Size i = 0, j = 0, i_end = name.len(); i < i_end; i++) {
	if (name.mem() [i] == ' ' ||
	    name.mem() [i] == '-')
	{
	    capital = true;
	    continue;
	}

	if (name.mem() [i] == '_') {
	    if (keep_underscore) {
		str->mem().mem() [j] = name.mem() [i];
		++j;
	    }
	    capital = true;
	    continue;
	}

	assert (j < str_len);

	if (capital) {
	    str->mem().mem() [j] = capitalizeSymbol (name.mem() [i]);
	    capital = false;
	} else
	    str->mem().mem() [j] = name.mem() [i];

	++j;
    }

    str->mem().mem() [str_len] = 0;

    return str;
}

StRef<String>
capitalizeNameAllCaps (ConstMemory const name)
{
    if (name.len() == 0)
        return st_grab (new (std::nothrow) String);

    assert (name.mem());

    StRef<String> const str = st_grab (new (std::nothrow) String);
    str->allocate (name.len());

    for (Size i = 0, i_end = name.len(); i < i_end; i++)
	str->mem().mem() [i] = capitalizeSymbol (name.mem() [i]);

    str->mem().mem() [name.len()] = 0;

    return str;
}

StRef<String>
lowercaseName (ConstMemory const name)
{
    if (name.len() == 0)
        return st_grab (new (std::nothrow) String);

    assert (name.mem());

    StRef<String> const str = st_grab (new (std::nothrow) String);
    str->allocate (name.len());

    for (Size i = 0, i_end = name.len(); i < i_end; i++) {
	if (name.mem() [i] == '-')
	    str->mem().mem() [i] = '_';
	else
	    str->mem().mem() [i] = lowercaseSymbol (name.mem() [i]);
    }

    str->mem().mem() [name.len()] = 0;

    return str;
}

}

