//--------------------------------------------------------------------------
// Copyright (C) 2015-2019 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------
// search_tool.cc author Russ Combs <rucombs@cisco.com>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cassert>

#include "detection/fp_config.h"
#include "framework/mpse.h"
#include "framework/mpse_batch.h"
#include "main/snort_config.h"
#include "search_tool.h"

namespace snort
{
SearchTool::SearchTool(const char* method, bool dfa)
{
    mpsegrp = new MpseGroup;

    // When no method is passed in, a normal search engine mpse will be created
    // with the search method the same as that configured for the fast pattern
    // normal search method, and also an offload search engine mpse will be
    // created with the search method the same as that configured for the fast
    // pattern offload search method.  If a method is passed in then an offload
    // search engine will not be created

    if (mpsegrp->create_normal_mpse(method))
    {
        if( dfa )
            mpsegrp->normal_mpse->set_opt(1);
    }

    if (method == nullptr)
    {
        if (mpsegrp->create_offload_mpse())
        {
            if ( dfa )
                mpsegrp->offload_mpse->set_opt(1);
        }
    }

    max_len = 0;
}

SearchTool::~SearchTool()
{
    delete mpsegrp;
}

void SearchTool::add(const char* pat, unsigned len, int id, bool no_case)
{
    add((const uint8_t*)pat, len, id, no_case);
}

void SearchTool::add(const char* pat, unsigned len, void* id, bool no_case)
{
    add((const uint8_t*)pat, len, id, no_case);
}

void SearchTool::add(const uint8_t* pat, unsigned len, int id, bool no_case)
{
    add(pat, len, (void*)(long)id, no_case);
}

void SearchTool::add(const uint8_t* pat, unsigned len, void* id, bool no_case)
{
    Mpse::PatternDescriptor desc(no_case, false, true);

    if ( mpsegrp->normal_mpse )
        mpsegrp->normal_mpse->add_pattern(nullptr,  pat, len, desc, id);
    if ( mpsegrp->offload_mpse )
        mpsegrp->offload_mpse->add_pattern(nullptr,  pat, len, desc, id);

    if ( len > max_len )
        max_len = len;
}

void SearchTool::prep()
{
    if ( mpsegrp->normal_mpse )
        mpsegrp->normal_mpse->prep_patterns(nullptr);
    if ( mpsegrp->offload_mpse )
        mpsegrp->offload_mpse->prep_patterns(nullptr);
}

int SearchTool::find(
    const char* str,
    unsigned len,
    MpseMatch mf,
    int& state,
    bool confine,
    void* user_data)
{
    int num = 0;
    SnortConfig* sc = SnortConfig::get_conf();
    FastPatternConfig* fp = sc->fast_pattern_config;

    if ( confine && max_len > 0 )
    {
        if ( max_len < len )
            len = max_len;
    }
    if ( !user_data )
        user_data = (void*)str;

    if ( fp and fp->get_offload_search_api() and (len >= sc->offload_limit) and
            (mpsegrp->get_offload_mpse() != mpsegrp->get_normal_mpse()) )
    {
        num = mpsegrp->get_offload_mpse()->search((const uint8_t*)str, len, mf, user_data, &state);

        if ( num < 0 )
            num = mpsegrp->get_normal_mpse()->search((const uint8_t*)str, len, mf, user_data,
                    &state);
    }
    else
        num = mpsegrp->get_normal_mpse()->search((const uint8_t*)str, len, mf, user_data, &state);

    // SeachTool::find expects the number found to be returned so if we have a failure return 0
    if ( num < 0 )
        num = 0;

    return num;
}

int SearchTool::find(
    const char* str,
    unsigned len,
    MpseMatch mf,
    bool confine,
    void* user_data)
{
    int state = 0;
    return find(str, len, mf, state, confine, user_data);
}

int SearchTool::find_all(
    const char* str,
    unsigned len,
    MpseMatch mf,
    bool confine,
    void* user_data)
{
    int num = 0;
    SnortConfig* sc = SnortConfig::get_conf();
    FastPatternConfig* fp = sc->fast_pattern_config;

    if ( confine && max_len > 0 )
    {
        if ( max_len < len )
            len = max_len;
    }
    if ( !user_data )
        user_data = (void*)str;

    int state = 0;

    if ( fp and fp->get_offload_search_api() and (len >= sc->offload_limit) and
            (mpsegrp->get_offload_mpse() != mpsegrp->get_normal_mpse()) )
    {
        num = mpsegrp->get_offload_mpse()->search_all((const uint8_t*)str, len, mf, user_data,
                &state);

        if ( num < 0 )
            num = mpsegrp->get_normal_mpse()->search_all((const uint8_t*)str, len, mf, user_data,
                    &state);
    }
    else
        num = mpsegrp->get_normal_mpse()->search_all((const uint8_t*)str, len, mf, user_data,
                &state);

    // SeachTool::find expects the number found to be returned so if we have a failure return 0
    if ( num < 0 )
        num = 0;

    return num;
}
} // namespace snort
