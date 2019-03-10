//--------------------------------------------------------------------------
// Copyright (C) 2014-2019 Cisco and/or its affiliates. All rights reserved.
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
// mpse.cc author Russ Combs <rucombs@cisco.com>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mpse.h"

#include <cassert>

#include "profiler/profiler_defs.h"
#include "search_engines/pat_stats.h"
#include "managers/mpse_manager.h"
#include "managers/module_manager.h"
#include "main/snort_config.h"
#include "detection/fp_config.h"

#include "mpse_batch.h"

using namespace std;

namespace snort
{
THREAD_LOCAL ProfileStats mpsePerfStats;

//-------------------------------------------------------------------------
// base stuff
//-------------------------------------------------------------------------

Mpse::Mpse(const char* m)
{
    method = m;
    verbose = 0;
    api = nullptr;
}

int Mpse::search(
    const unsigned char* T, int n, MpseMatch match,
    void* context, int* current_state)
{
    Profile profile(mpsePerfStats);
    pmqs.matched_bytes += n;
    return _search(T, n, match, context, current_state);
}

int Mpse::search_all(
    const unsigned char* T, int n, MpseMatch match,
    void* context, int* current_state)
{
    Profile profile(mpsePerfStats);
    pmqs.matched_bytes += n;
    return _search(T, n, match, context, current_state);
}

void Mpse::search(MpseBatch& batch, MpseType mpse_type)
{
    int start_state;

    for ( auto& item : batch.items )
    {
        if (item.second.done)
            continue;

        item.second.error = false;
        item.second.matches = 0;

        for ( auto& so : item.second.so )
        {
            start_state = 0;
            switch (mpse_type)
            {
                case MPSE_TYPE_NORMAL:
                    item.second.matches += so->get_normal_mpse()->search(item.first.buf,
                            item.first.len, batch.mf, batch.context, &start_state);
                    break;
                case MPSE_TYPE_OFFLOAD:
                    item.second.matches += so->get_offload_mpse()->search(item.first.buf,
                            item.first.len, batch.mf, batch.context, &start_state);
                    break;
            }
        }
        item.second.done = true;
    }
}

}

