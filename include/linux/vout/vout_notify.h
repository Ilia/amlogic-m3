/*
 * Amlogic Apollo
 * frame buffer driver
 *
 * Copyright (C) 2009 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:	jianfeng_wang
 *
 */
#ifndef  VOUT_NOTIFY_H
#define  VOUT_NOTIFY_H

#include <linux/notifier.h>
#include "vinfo.h"

typedef struct 
{
	const vinfo_t* (*get_vinfo)(void);
	int		 (*set_vmode)(vmode_t);
	vmode_t  (*validate_vmode)(char *);
}vout_op_t ;

typedef struct 
{
	char  	  *name;
	vout_op_t  op;
} vout_server_t;

#define	INIT_TV_MODULE_SERVER(server_name) \
	vout_server_t server_name ={ \
		.name=NULL, \
		.op ={ \
			.get_vinfo = NULL, \
		},\
	}

extern int vout_register_client(struct notifier_block * ) ;
extern int vout_unregister_client(struct notifier_block *) ;
extern int vout_register_server(vout_server_t *);
extern int vout_unregister_server(void) ;
extern int vout_notifier_call_chain(unsigned long, void *) ;

extern const vinfo_t *get_current_vinfo(void);
extern vmode_t get_current_vmode(void);
extern int set_current_vmode(vmode_t);
extern vmode_t validate_vmode(char *);

#define VOUT_EVENT_MODE_CHANGE		0x00010000	
#define VOUT_EVENT_OSD_BLANK		0x00020000
#define VOUT_EVENT_OSD_DISP_AXIS	0x00020001 

#endif /* VOUT_NOTIFY_H */
