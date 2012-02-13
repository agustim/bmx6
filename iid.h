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

#ifndef _IID_H
#define _IID_H



#define IID_REPOS_SIZE_BLOCK 32

#define IID_REPOS_SIZE_MAX  ((IID_T)(-1))
#define IID_REPOS_SIZE_WARN 1024

#define IID_RSVD_UNUSED 0 // this iid value is reserved to indicate an unused iid variable
#define IID_MIN_USABLE  1 // this is the minimum usable iid value to represent a valid reference to a dhn

#define IID_SPREAD_FK   1  /*default=2 , 1 means no spreading    #define IID_REPOS_USAGE_WARNING 10 */



#define IID_DHASH_PURGE_TO 300000
#define IID_DHASH_VALIDITY_TO (IID_DHASH_PURGE_TO - (IID_DHASH_PURGE_TO / 10))



#define MIN_IID_TABLES 0
#define TYP_IID_TABLES_OFF 0
#define TYP_IID_TABLES_ON 1
#define TYP_IID_TABLES_ADAPTIVE 2
#define MAX_IID_TABLES 2
#define DEF_IID_TABLES 2
#define ARG_IID_TABLES "iidTables"

extern int32_t iid_tables_self;
extern int32_t iid_tables_neigh;



struct iid_ref {
	IID_T myIID4x;
	uint16_t referred_by_neigh_timestamp_sec;
};

struct iid_repos {
	IID_T arr_size; // the number of allocated array fields
	IID_T min_free; // the first unused array field from the beginning of the array (might be outside of allocated space)
//	IID_T max_free; // the first unused array field after the last used field in the array (might be outside of allocated space)
	IID_T max_used; // the last used field in the array
	IID_T tot_used; // the total number of used fields in the array

	IID_T neighIID4neigh;
	uint16_t referred_by_neigh_timestamp_sec;

	union {
		uint8_t *u8;
		IID_NODE_T **node;
		struct iid_ref *ref;
	} arr;
};

extern struct iid_repos my_iid_repos;



//int8_t iid_extend_repos( struct iid_repos *rep );

void iid_purge_repos( struct iid_repos *rep, TIME_T timeout );

void iid_free(struct iid_repos *rep, IID_T iid);

void iid_free_neighIIDrepos_from_myIID4x( struct iid_repos *rep, IID_T myIID4x);

IDM_T iid_set_neighIID4x(struct iid_repos *neigh_rep, IID_T neighIID4x, IID_T myIID4x);

IID_T iid_new_myIID4x( IID_NODE_T *dhn );


IID_NODE_T* iid_get_node_by_neighIID4x(IID_NEIGH_T *nn, IID_T neighIID4x, IDM_T verbose);


IID_NODE_T* iid_get_node_by_myIID4x( IID_T myIID4x );

void init_iid( void );

void iid_tables_check_usage(void);

#endif
