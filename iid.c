/*
 * Copyright (c) 2010  Axel Neumann
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "bmx.h"
#include "iid.h"
#include "tools.h"

#define CODE_CATEGORY_NAME "iid"

struct iid_repos my_iid_repos = { 0,0,0,0,0,0, {NULL} };
int32_t iid_tables = DEF_IID_TABLES;

int8_t iid_extend_repos(struct iid_repos *rep)
{
        TRACE_FUNCTION_CALL;

        dbgf_all(DBGT_INFO, "sizeof iid: %zu,  tot_used %d  arr_size %d ",
                (rep == &my_iid_repos) ? sizeof (IID_NODE_T*) : sizeof (IID_T), rep->tot_used, rep->arr_size);

        assertion(-500217, (rep != &my_iid_repos || IID_SPREAD_FK != 1 || (rep->tot_used ) == rep->arr_size));
        assertion(-501410, IMPLIES(!iid_tables, rep == &my_iid_repos));

        if (rep->arr_size + IID_REPOS_SIZE_BLOCK >= IID_REPOS_SIZE_WARN) {

                dbgf_sys(DBGT_WARN, "%d", rep->arr_size);

                if (rep->arr_size + IID_REPOS_SIZE_BLOCK >= IID_REPOS_SIZE_MAX)
                        return FAILURE;
        }

        int field_size = (rep == &my_iid_repos) ? sizeof (IID_NODE_T*) : sizeof (struct iid_ref);

        if (rep->arr_size) {

                rep->arr.u8 = debugRealloc(rep->arr.u8, (rep->arr_size + IID_REPOS_SIZE_BLOCK) * field_size, -300035);

        } else {

                rep->arr.u8 = debugMalloc(IID_REPOS_SIZE_BLOCK * field_size, -300085);
                rep->tot_used = IID_MIN_USABLE;
                rep->min_free = IID_MIN_USABLE;
                rep->max_used = (IID_MIN_USABLE - 1);
        }

        memset(&(rep->arr.u8[rep->arr_size * field_size]), 0, IID_REPOS_SIZE_BLOCK * field_size);

        rep->arr_size += IID_REPOS_SIZE_BLOCK;

        return SUCCESS;
}


void iid_purge_repos( struct iid_repos *rep, TIME_T timeout )
{
        TRACE_FUNCTION_CALL;

        if (timeout) {

                if (rep->neighIID4neigh >= IID_MIN_USABLE &&
                        (((uint16_t) (((uint16_t) bmx_time_sec) - rep->referred_by_neigh_timestamp_sec) >= timeout / 1000))) {
                        rep->referred_by_neigh_timestamp_sec = 0;
                        rep->neighIID4neigh = IID_RSVD_UNUSED;
                }

                if (rep->tot_used > IID_MIN_USABLE ) {

                        IID_T iid;
                        for (iid = IID_MIN_USABLE; iid <= rep->max_used; iid++) {

                                struct iid_ref *ref = &rep->arr.ref[iid];

                                if (ref->myIID4x >= IID_MIN_USABLE &&
                                        ((uint16_t) (((uint16_t) bmx_time_sec) - ref->referred_by_neigh_timestamp_sec) >= timeout / 1000))
                                        iid_free(rep, iid);

                        }
                }

        } else {

                if (rep->arr.u8)
                        debugFree(rep->arr.u8, -300135);

                memset(rep, 0, sizeof ( struct iid_repos));
        }
}

void iid_free(struct iid_repos *rep, IID_T iid)
{
        TRACE_FUNCTION_CALL;
        int m = (rep == &my_iid_repos);

        assertion(-500330, (iid >= IID_MIN_USABLE));
        assertion(-500228, (iid < rep->arr_size && iid <= rep->max_used && rep->tot_used > IID_MIN_USABLE));
        assertion(-500229, ((m ? (rep->arr.node[iid] != NULL) : (rep->arr.ref[iid].myIID4x) != 0)));

        if (m) {
                rep->arr.node[iid] = NULL;
        } else {
                rep->arr.ref[iid].myIID4x = IID_RSVD_UNUSED;
                rep->arr.ref[iid].referred_by_neigh_timestamp_sec = 0;
        }

        rep->min_free = MIN(rep->min_free, iid);

        if (rep->max_used == iid) {

                IID_T i;

                for (i = iid; i > IID_MIN_USABLE; i--) {

                        if (m ? (rep->arr.node[i - 1] != NULL) : (rep->arr.ref[i - 1].myIID4x) != 0)
                                break;
                }

                rep->max_used = i - 1;
        }

        rep->tot_used--;

        dbgf_all( DBGT_INFO, "mine=%d, iid=%d tot_used=%d, min_free=%d max_used=%d",
                m, iid, rep->tot_used, rep->min_free, rep->max_used);

        if (rep->tot_used <= IID_MIN_USABLE) {

                assertion(-500362, (rep->tot_used == IID_MIN_USABLE && rep->max_used == (IID_MIN_USABLE - 1) && rep->min_free == IID_MIN_USABLE));

                iid_purge_repos( rep, 0 );

        }

}

IID_NODE_T* iid_get_node_by_myIID4x(IID_T myIID4x)
{
        TRACE_FUNCTION_CALL;

        if ( my_iid_repos.max_used < myIID4x )
                return NULL;

        IID_NODE_T *dhn = my_iid_repos.arr.node[myIID4x];

        assertion(-500328, IMPLIES(dhn, dhn->myIID4orig == myIID4x));

        if (dhn && !dhn->on) {

                dbgf_track(DBGT_INFO, "myIID4x %d INVALIDATED %d sec ago",
                        myIID4x, (bmx_time - dhn->referred_by_me_timestamp) / 1000);

                return NULL;
        }


        return dhn;
}


IID_NODE_T* iid_get_node_by_neighIID4x(IID_NEIGH_T *neigh, IID_T neighIID4x, IDM_T verbose)
{
        TRACE_FUNCTION_CALL;

        assertion(-501411, (neigh));

        struct iid_repos *rep = &neigh->neighIID4x_repos;

        assertion(-501412, (iid_tables ? rep->neighIID4neigh == IID_RSVD_UNUSED : rep->tot_used <= IID_MIN_USABLE));

        if (!iid_tables) {

                if (neigh->neighIID4x_repos.neighIID4neigh == neighIID4x &&
                        (((uint16_t) (((uint16_t) bmx_time_sec) - neigh->neighIID4x_repos.referred_by_neigh_timestamp_sec) < IID_DHASH_VALIDITY_TO / 1000))) {

                        neigh->neighIID4x_repos.referred_by_neigh_timestamp_sec = bmx_time_sec;
                        return neigh->dhn;
                }
                return NULL;
        }

        if (neigh->neighIID4x_repos.max_used < neighIID4x) {

                if (verbose) {
                        dbgf_all(DBGT_INFO, "NB: global_id=%s neighIID4x=%d to large for neighIID4x_repos",
                                neigh ? globalIdAsString(&neigh->dhn->on->global_id) : "???", neighIID4x);
                }
                return NULL;
        }

        struct iid_ref *ref = &(neigh->neighIID4x_repos.arr.ref[neighIID4x]);


        if (ref->myIID4x < IID_MIN_USABLE) {

                if (verbose) {
                        dbgf_all(DBGT_WARN, "neighIID4x=%d not recorded by neighIID4x_repos", neighIID4x);
                }

        } else

                if ((uint16_t) (((uint16_t) bmx_time_sec) - ref->referred_by_neigh_timestamp_sec) >= IID_DHASH_VALIDITY_TO / 1000) {

                if (verbose) {
                        dbgf_track(DBGT_WARN, "neighIID4x=%d outdated in neighIID4x_repos, now_sec=%d, ref_sec=%d",
                                neighIID4x, bmx_time_sec, ref->referred_by_neigh_timestamp_sec);
                }

        } else {

                ref->referred_by_neigh_timestamp_sec = bmx_time_sec;

                if (ref->myIID4x <= my_iid_repos.max_used) {

                        IID_NODE_T *dhn = my_iid_repos.arr.node[ref->myIID4x];

                        if (dhn) {

                                return dhn;
                        }

                        if (verbose) {
                                dbgf_track(DBGT_WARN, "neighIID4x=%d -> myIID4x=%d empty!", neighIID4x, ref->myIID4x);
                        }

                } else {

                        if (verbose) {
                                dbgf_track(DBGT_WARN, "neighIID4x=%d -> myIID4x=%d to large!", neighIID4x, ref->myIID4x);
                        }
                }
        }

        return NULL;
}


STATIC_FUNC
void _iid_set(struct iid_repos *rep, IID_T repIId4x, IID_T myIID4x, IID_NODE_T *dhn)
{
        TRACE_FUNCTION_CALL;
        assertion(-501413, IMPLIES(!iid_tables, rep == &my_iid_repos));
        assertion(-500535, (repIId4x >= IID_MIN_USABLE));
        assertion(-501415, (repIId4x < rep->arr_size ));

        assertion(-500530, (XOR(myIID4x >= IID_MIN_USABLE, dhn))); // eihter the one ore the other !!
        assertion(-501416, IMPLIES(rep == &my_iid_repos, dhn));
        assertion(-501417, IMPLIES(rep != &my_iid_repos, myIID4x >= IID_MIN_USABLE));

        if (myIID4x) {
                rep->arr.ref[repIId4x].myIID4x = myIID4x;
                rep->arr.ref[repIId4x].referred_by_neigh_timestamp_sec = bmx_time_sec;
        } else {
                rep->arr.node[repIId4x] = dhn;
        }

        rep->tot_used++;
        rep->max_used = MAX( rep->max_used, repIId4x );

        if (rep->min_free == repIId4x) {

                IID_T min_free = rep->min_free + 1;

                for (; min_free < rep->arr_size; min_free++) {

                        if (myIID4x ? !(rep->arr.ref[min_free].myIID4x) : !(rep->arr.node[min_free]))
                                break;
                }

                rep->min_free = min_free;
        }

        assertion(-500244, (rep->min_free <= rep->max_used + 1));


}


IID_T iid_new_myIID4x(IID_NODE_T *dhn)
{
        TRACE_FUNCTION_CALL;
        IID_T myIID4x;
#ifndef NO_ASSERTIONS
        IDM_T warn = 0;
#endif

        assertion(-500216, (my_iid_repos.tot_used <= my_iid_repos.arr_size));

        while (my_iid_repos.arr_size <= my_iid_repos.tot_used * IID_SPREAD_FK)
                iid_extend_repos( &my_iid_repos );

        if (IID_SPREAD_FK > 1) {

                uint32_t random = rand_num(my_iid_repos.arr_size);

                // Never put random function into MAX()! It would be called twice
                myIID4x = MAX(IID_MIN_USABLE, random);

                while (my_iid_repos.arr.node[myIID4x]) {

                        myIID4x++;
                        if (myIID4x >= my_iid_repos.arr_size) {

                                myIID4x = IID_MIN_USABLE;

                                assertion(-500533, (!(warn++)));
                        }
                }

        } else {
                
                myIID4x = my_iid_repos.min_free;
        }

        _iid_set(&my_iid_repos, myIID4x, 0, dhn);

        return myIID4x;

}


IDM_T iid_set_neighIID4x(struct iid_repos *rep, IID_T neighIID4x, IID_T myIID4x)
{
        TRACE_FUNCTION_CALL;
        assertion(-500326, (neighIID4x >= IID_MIN_USABLE));
        assertion(-500327, (myIID4x >= IID_MIN_USABLE));
        assertion(-500384, (rep && rep != &my_iid_repos));
        assertion(-500245, (my_iid_repos.max_used >= myIID4x));

        assertion(-501418, (iid_tables ? rep->neighIID4neigh == IID_RSVD_UNUSED : rep->tot_used <= IID_MIN_USABLE));

        IID_NODE_T *dhn = my_iid_repos.arr.node[myIID4x];

        assertion(-501437, (dhn && dhn->on));

        if ( !iid_tables ) {
                if (dhn->neigh && &dhn->neigh->neighIID4x_repos == rep) {
                        rep->neighIID4neigh = neighIID4x;
                        rep->referred_by_neigh_timestamp_sec = bmx_time_sec;
                }
                return SUCCESS;
        }

        if (rep->max_used >= neighIID4x) {

                struct iid_ref *ref = &(rep->arr.ref[neighIID4x]);

                if (ref->myIID4x >= IID_MIN_USABLE) {

                        if (ref->myIID4x == myIID4x ||
                                (((uint16_t) (((uint16_t) bmx_time_sec) - ref->referred_by_neigh_timestamp_sec)) >= IID_DHASH_VALIDITY_TO / 1000)) {

                                ref->myIID4x = myIID4x;
                                ref->referred_by_neigh_timestamp_sec = bmx_time_sec;

                                return SUCCESS;
                        }

                        IID_NODE_T *dhn_old;
                        dhn_old = my_iid_repos.arr.node[ref->myIID4x]; // avoid -DNO_DEBUG_SYS warnings
                        dbgf_sys(DBGT_ERR, "demanding mapping: neighIID4x=%d to myIID4x=%d "
                                "(global_id=%s updated=%d last_referred_by_me=%d)  "
                                "already used for ref->myIID4x=%d (last_referred_by_neigh_sec=%d %s=%s last_referred_by_me=%jd)! Reused faster than allowed!!",
                                neighIID4x, myIID4x, globalIdAsString(&dhn->on->global_id), dhn->on->updated_timestamp,
                                dhn->referred_by_me_timestamp,
                                ref->myIID4x,
                                ref->referred_by_neigh_timestamp_sec,
                                (!dhn_old ? "???" : (dhn_old->on ? globalIdAsString(&dhn_old->on->global_id) :
                                (is_zero(&dhn_old->dhash, sizeof (dhn_old->dhash)) ? "FREED" : "INVALIDATED"))),
                                dhn_old ? memAsHexString(&dhn_old->dhash, sizeof ( dhn_old->dhash)) : "???",
                                dhn_old ? (int64_t)dhn_old->referred_by_me_timestamp : -1
                                );

//                        EXITERROR(-500701, (0));

                        return FAILURE;
                }

                assertion(-500242, (ref->myIID4x == IID_RSVD_UNUSED));
        }


        while (rep->arr_size <= neighIID4x) {

                if (
                        rep->arr_size > IID_REPOS_SIZE_BLOCK &&
                        rep->arr_size > my_iid_repos.arr_size &&
                        rep->tot_used < rep->arr_size / (2 * IID_SPREAD_FK)) {

                        dbgf_sys(DBGT_WARN, "IID_REPOS USAGE WARNING neighIID4x %d myIID4x %d arr_size %d used %d",
                                neighIID4x, myIID4x, rep->arr_size, rep->tot_used );
                }

                iid_extend_repos(rep);
        }

        assertion(-500243, ((rep->arr_size > neighIID4x &&
                (rep->max_used + 1 <= neighIID4x || rep->arr.ref[neighIID4x].myIID4x == IID_RSVD_UNUSED))));

        _iid_set( rep, neighIID4x, myIID4x, NULL);

        return SUCCESS;
}


void iid_free_neighIIDrepos_from_myIID4x( struct iid_repos *neigh_rep, IID_T free_myIID4x)
{
        TRACE_FUNCTION_CALL;
        assertion(-500282, (neigh_rep != &my_iid_repos));
        assertion(-500328, (free_myIID4x >= IID_MIN_USABLE));
        assertion(-501419, (iid_tables ? neigh_rep->neighIID4neigh == IID_RSVD_UNUSED : neigh_rep->tot_used <= IID_MIN_USABLE));

        assertion(-501444, (((my_iid_repos.arr.node[free_myIID4x]))));
        assertion(-501445, (!(my_iid_repos.arr.node[free_myIID4x]->on)));
        assertion(-501446, (!(my_iid_repos.arr.node[free_myIID4x]->neigh)));


/*
        if (free_dhn->neigh && &free_dhn->neigh->neighIID4x_repos == neigh_rep) {

                assertion(-501420, 0); // would result in a  described neighbor with unknown neighIID4neigh

                if (iid_tables) {
                        neigh_rep->neighIID4neigh = IID_RSVD_UNUSED;
                        neigh_rep->referred_by_neigh_timestamp_sec = 0;
                        return;
                }
        }
*/


        IID_T p;
        uint16_t removed = 0;

        for (p = IID_MIN_USABLE; p <= neigh_rep->max_used; p++) {

                if (neigh_rep->arr.ref[p].myIID4x == free_myIID4x) {

                        if (removed++) {
                                // there could indeed be several (if the neigh has timeouted this node and learned it again later)
                                dbgf(DBGL_TEST, DBGT_INFO, "removed %d. stale rep->arr.sid[%d] = %d", removed, p, free_myIID4x);
                        }

                        iid_free(neigh_rep, p);
                }
        }
}



static int32_t opt_iid_tables ( uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn )
{

        static int32_t prev_iid_tables = DEF_IID_TABLES;

        if (cmd == OPT_APPLY) {

                //assertion(-501421, (iid_tables == strtol(patch->val, NULL, 10)));
                //iid_tables = !iid_tables;
                dbgf_track(DBGT_INFO, "(new) iid_tables=%d prev_iid_tables=%d", iid_tables, prev_iid_tables);

                if (prev_iid_tables == iid_tables)
                        return SUCCESS;
                else
                        prev_iid_tables = iid_tables;


                struct avl_node *an;
                struct neigh_node *neigh;

                
                for (an = NULL; (neigh = avl_iterate_item(&neigh_tree, &an));) {

                        assertion(-501420, (neigh->dhn));

                        struct iid_repos *rep = &neigh->neighIID4x_repos;

                        if (iid_tables) {

                                assertion(-501423, (rep->arr_size == 0));
                                assertion(-501424, (rep->neighIID4neigh >= IID_MIN_USABLE));
                                assertion(-501425, (neigh->dhn->myIID4orig >= IID_MIN_USABLE));

                                IID_T neighIID4neigh = rep->neighIID4neigh;
                                uint16_t referred_by_neigh_timestamp_sec = rep->referred_by_neigh_timestamp_sec;

                                rep->referred_by_neigh_timestamp_sec = 0;
                                rep->neighIID4neigh = IID_RSVD_UNUSED;
                                iid_set_neighIID4x(rep, neighIID4neigh, neigh->dhn->myIID4orig);
                                rep->arr.ref[neighIID4neigh].referred_by_neigh_timestamp_sec = referred_by_neigh_timestamp_sec;

                        } else {

                                //then purge all distributed iid tables and save neighIID4neigh

                                IID_T neighIID4neigh = IID_RSVD_UNUSED;
                                IID_T neighIID4x = IID_RSVD_UNUSED;
                                uint16_t referred_by_neigh_timestamp_sec = 0;

                                for (neighIID4x = IID_MIN_USABLE; neighIID4x <= rep->max_used; neighIID4x++) {

                                        struct iid_ref *ref = &rep->arr.ref[neighIID4x];

                                        if (ref->myIID4x == neigh->dhn->myIID4orig) {

                                                if ((uint16_t) (((uint16_t) bmx_time_sec) - rep->referred_by_neigh_timestamp_sec) < IID_DHASH_VALIDITY_TO / 1000) {
                                                        assertion(-501422, (neighIID4neigh == IID_RSVD_UNUSED));
                                                        referred_by_neigh_timestamp_sec = ref->referred_by_neigh_timestamp_sec;
                                                        neighIID4neigh = neighIID4x;
                                                }
                                        }
                                }

                                iid_purge_repos(rep, 0);

                                rep->neighIID4neigh = neighIID4neigh;
                                rep->referred_by_neigh_timestamp_sec = referred_by_neigh_timestamp_sec;
                        }
                }
        }

	return SUCCESS;
}

static struct opt_type iid_options[]=
{
//        ord parent long_name          shrt Attributes				*ival		min		max		default		*func,*syntax,*help

	{ODI,0,ARG_IID_TABLES, 	        0, 5,0,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&iid_tables,	MIN_IID_TABLES,    MAX_IID_TABLES,    DEF_IID_TABLES,0,  opt_iid_tables,
			ARG_VALUE_FORM,	"enable/disable support for distributed IID tables "}

};


void init_iid( void )
{
	register_options_array( iid_options, sizeof( iid_options ), CODE_CATEGORY_NAME );

}
