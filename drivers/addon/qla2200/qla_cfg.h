/******************************************************************************
 *                  QLOGIC LINUX SOFTWARE
 *
 * QLogic ISP2x00 device driver for Linux 2.2.x and 2.4.x
 * Copyright (C) 2001 Qlogic Corporation
 * (www.qlogic.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 ******************************************************************************/

/*
 * QLogic ISP2x00 Multi-path LUN Support
 * Multi-path include file.
 */

#ifndef _QLA_CFG_H
#define	_QLA_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Temp definitions.
 */
#include "qla_def.h"

/*
 * Failover definitions
 */
#define FAILOVER_TYPE_COUNT		4
#define MP_NOTIFY_RESET_DETECTED	1
#define MP_NOTIFY_PWR_LOSS		2
#define MP_NOTIFY_LOOP_UP		3
#define MP_NOTIFY_LOOP_DOWN		4
#define MP_NOTIFY_BUS_RESET		5
#define FAILOVER_TYPE_ERROR_RETRY	1
#define MAX_NUMBER_PATHS		FO_MAX_PATHS
#define PORT_NAME_SIZE			WWN_SIZE
#define FAILOVER_NOTIFY_STATUS_ERROR	QLA2X00_SUCCESS
#define FAILOVER_NOTIFY_STATUS_SUCCESS  QLA2X00_SUCCESS
#define FAILOVER_NOTIFY_CDB_LENGTH_MAX	FO_NOTIFY_CDB_LENGTH_MAX
#define MAX_TARGETS_PER_DEVICE		SDM_DEF_MAX_TARGETS_PER_DEVICE

/*
 * Limits definitions.
 */
#define MAX_LUNS_PER_DEVICE	MAX_LUNS		/* Maximum number of luns */
#define MAX_MP_DEVICES		MAX_TARGETS		/* Maximum number of virtual devices */
#define MAX_PATHS_PER_DEVICE	8			/* Maximum number of paths */
#ifndef MAX_LUNS
#define	MAX_LUNS		256
#endif
#define MAX_HOSTS		MAX_HOST_COUNT

/* Async notification types */
#define NOTIFY_EVENT_LINK_DOWN      1			/* Link went down */
#define NOTIFY_EVENT_LINK_UP        2			/* Link is back up */
#define NOTIFY_EVENT_RESET_DETECTED 3			/* Reset detected */

/*
 * Macros
 */
#if defined(linux)

#define PRINTK			printk
#define QL_PRINT(s)		printk(KERN_INFO s);
#define QL_ERROR(s)		printk(KERN_INFO s);

#if DEBUG
#define QL_DEBUG(s)		printk(s);
#else
#define QL_DEBUG(s)
#endif

/* MACROS */
#define qla2x00_is_portname_equal(N1,N2) \
    ((memcmp((N1),(N2),WWN_SIZE)==0?TRUE:FALSE))
#define qla2x00_is_nodename_equal(N1,N2) \
    ((memcmp((N1),(N2),WWN_SIZE)==0?TRUE:FALSE))
#if 0
#define qla2x00_allocate_path_list() \
    ((mp_path_list_t *)KMEM_ZALLOC(sizeof(mp_path_list_t)))
#endif

#else	/* Must be Solaris */

#define  KMEM_ZALLOC(siz)	kmem_zalloc((siz), KM_SLEEP)
#define  KMEM_FREE(ip,siz)	kmem_free((ip),(siz))
#define  BZERO(ptr, amt)	bzero((void *)(ptr), (amt))
/* fixme(dg) called BCOPY(dst,src,amt)  */
#define  BCOPY(src, dst, amt)	bcopy((const void *)(src), (void *)(dst), (amt))
#define  BCMP(src, dst, amt)	bcmp((const void *)(dst), (const void *)(src), (amt))

#endif

/*
 * Per-multipath driver parameters
 */
typedef struct _mp_lun_data {
	uint8_t 	data[MAX_LUNS];
#define LUN_DATA_ENABLED		BIT_7
#define LUN_DATA_PREFERRED_PATH		BIT_6
} mp_lun_data_t;


#define PATH_INDEX_INVALID		0xff

/*
 * Per-device collection of all paths.
 */
typedef struct _mp_path_list {
	struct _mp_path *last;		/* ptrs to end of circular list of paths */
	uint8_t		path_cnt;	/* number of paths */
	uint8_t		visible;	/* visible path */
	uint16_t	reserved1;	/* Memory alignment */
	uint32_t	reserved2;	/* Memory alignment */
	uint8_t		current_path[ MAX_LUNS_PER_DEVICE ]; /* current path for a given lun */
	uint16_t	failover_cnt[ FAILOVER_TYPE_COUNT ];
} mp_path_list_t;

/*
 * Definitions for failover notify SRBs.  These SRBs contain
 * failover notify CDBs to notify a target that a failover has
 * occurred.
 */
typedef struct _failover_notify_srb {
	srb_t		*srb;
	uint16_t	status;
	uint16_t	reserved;
} failover_notify_srb_t;

/*
 * Per-device multipath control data.
 */
typedef struct _mp_device {
	mp_path_list_t	*path_list;		/* Path list for device.  */
    int				dev_id;
    	int			use_cnt;	/* number of users */
	uint8_t         nodename[WWN_SIZE];	/* World-wide node name. */
			/* World-wide port names. */
	uint8_t         portnames[MAX_PATHS_PER_DEVICE][WWN_SIZE];
} mp_device_t;

/*
 * Per-adapter multipath Host
 */
typedef struct _mp_host {
	struct _mp_host	*next;	/* ptr to next host adapter in list */
	adapter_state_t	*ha;	/* ptr to lower-level driver adapter struct */
	int		instance;	/* OS instance number */
	fc_port_t	*fcport;	/* Port chain for this adapter */
	mp_device_t	*mp_devs[MAX_MP_DEVICES]; /* Multipath devices */

	uint32_t	flags;
#define MP_HOST_FLAG_NEEDS_UPDATE  BIT_0  /* Need to update device data. */
#define MP_HOST_FLAG_FO_ENABLED	   BIT_1  /* Failover enabled for this host */
#define MP_HOST_FLAG_DISABLE	   BIT_2  /* Bypass qla_cfg. */

	uint8_t		nodename[WWN_SIZE];
	uint8_t		portname[WWN_SIZE];
	uint16_t	MaxLunsPerTarget;

	/* Not sure what this is for, in NT code ??*/
	uint16_t	relogin_countdown;
} mp_host_t;

/*
 * Describes path a single.
 */
typedef struct _mp_path {
	struct _mp_path  *next;			/* next path in list  */
	struct _mp_host *host;			/* Pointer to adapter */
	fc_port_t	*port;			/* FC port info  */
	uint16_t       id;			/* Path id (index) */
	uint8_t        mp_byte;			/* Multipath control byte */
#define MP_MASK_HIDDEN	0x80
#define MP_MASK_UNCONFIGURED	0x40
#define MP_MASK_PRIORITY	0x07

	uint8_t        relogin;			/* Need to relogin to port */
	uint8_t        config;			/* User configured path	*/
	uint8_t        reserved[3];
	mp_lun_data_t	lun_data;		/* Lun data information */
	uint8_t        portname[WWN_SIZE];	/* Port name of this target. */
} mp_path_t;



/*
 * Failover notification requests from host driver.
 */
typedef struct failover_notify_entry {
	struct scsi_address		*os_addr;
} failover_notify_t;


#endif /* _QLA_CFG_H */
