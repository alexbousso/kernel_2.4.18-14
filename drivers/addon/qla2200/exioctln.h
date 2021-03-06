/*****************************************************************************
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
****************************************************************************/

/* 
   Rev 9     July 26, 2001	RL
             - Added definition of signed types.

   Rev 8     July 05, 2001	RL
             - Redefined ioctl command values.

   Rev 7     Nov 06, 2000   BN
             - Added EXT_DEF_MAX_AEN_QUEUE_OS define
             - Added define for handle_hba_t

   Rev 6     Oct 25, 2000   BN
             - Added EXT_CC_DRIVER_PROP_OS define

   Rev 5     Oct 25, 2000   BN
             - Redo the copyright header and add AEN details

   Rev 4     Oct 23, 2000   BN
             - Added definition for BOOLEAN

   Rev 3     Oct 23, 2000   BN
             - Added definitions for EXT_ADDR_MODE_OS
               and also include of <linux/ioctl.h>

   Rev 2     Oct 18, 2000   BN
             - Enable API Exention support

   Rev 1     Original version Sep 7, 2000   BN

*/


#ifndef _EXIOCT_LN_H_
#define _EXIOCT_LN_H_

#include <linux/ioctl.h>

#ifdef APILIB
#include <stdint.h>
#endif


#define	INT8	int8_t
#define	INT16	int16_t
#define	INT32	int32_t
#define	UINT8	uint8_t
#define	UINT16	uint16_t
#define	UINT32	uint32_t
#define	UINT64	void *
#define BOOLEAN uint8_t

typedef struct  track_instance
{

  int   handle;

} track_instance_t;


#if BITS_PER_LONG <= 32
#define EXT_ADDR_MODE_OS  EXT_DEF_ADDR_MODE_32
#else
#define EXT_ADDR_MODE_OS  EXT_DEF_ADDR_MODE_64
#endif


#define QLMULTIPATH_MAGIC 'y'

#define _QLBUILD   /* for exioct.h to enable include of qinsdmgt.h */



#define	EXT_DEF_MAX_HBA_OS		256	/* 0 - 0xFF */
#define	EXT_DEF_MAX_BUS_OS		1
#define	EXT_DEF_MAX_TARGET_OS		256	/* 0 - 0xFF */
#define	EXT_DEF_MAX_LUN_OS		256	/* 0 - 0xFF */

#define EXT_DEF_MAX_AEN_QUEUE_OS        64



/*****************/
/* Command codes */
/*****************/

/* These are regular command codes */

#define EXT_CC_QUERY_OS					/* QUERY */	\
    _IOWR(QLMULTIPATH_MAGIC, 0x00, sizeof(EXT_IOCTL))
#define EXT_CC_SEND_FCCT_PASSTHRU_OS			/* FCCT_PASSTHRU */ \
    _IOWR(QLMULTIPATH_MAGIC, 0x01, sizeof(EXT_IOCTL))
#define	EXT_CC_REG_AEN_OS				/* REG_AEN */	\
    _IOWR(QLMULTIPATH_MAGIC, 0x02, sizeof(EXT_IOCTL))
#define	EXT_CC_GET_AEN_OS				/* GET_AEN */	\
    _IOWR(QLMULTIPATH_MAGIC, 0x03, sizeof(EXT_IOCTL))
#define	EXT_CC_SEND_ELS_RNID_OS				/* SEND_ELS_RNID */ \
    _IOWR(QLMULTIPATH_MAGIC, 0x04, sizeof(EXT_IOCTL))
#define	EXT_CC_SCSI_PASSTHRU_OS				/* SCSI_PASSTHRU */ \
    _IOWR(QLMULTIPATH_MAGIC, 0x05, sizeof(EXT_IOCTL))

#define EXT_CC_GET_DATA_OS				/* GET_DATA */	\
    _IOWR(QLMULTIPATH_MAGIC, 0x06, sizeof(EXT_IOCTL))
#define EXT_CC_SET_DATA_OS				/* SET_DATA */	\
    _IOWR(QLMULTIPATH_MAGIC, 0x07, sizeof(EXT_IOCTL))

/* following are internal command codes. */
#define EXT_CC_RESERVED0A_OS						\
    _IOWR(QLMULTIPATH_MAGIC, 0x08, sizeof(EXT_IOCTL))
#define EXT_CC_RESERVED0B_OS						\
    _IOWR(QLMULTIPATH_MAGIC, 0x09, sizeof(EXT_IOCTL))

#define EXT_CC_RESERVED0C_OS						\
    _IOWR(QLMULTIPATH_MAGIC, 0x0a, sizeof(EXT_IOCTL))
#define EXT_CC_RESERVED0D_OS						\
    _IOWR(QLMULTIPATH_MAGIC, 0x0b, sizeof(EXT_IOCTL))

#define EXT_CC_RESERVED0E_OS						\
    _IOWR(QLMULTIPATH_MAGIC, 0x0c, sizeof(EXT_IOCTL))
#define EXT_CC_RESERVED0F_OS						\
    _IOWR(QLMULTIPATH_MAGIC, 0x0d, sizeof(EXT_IOCTL))

#define EXT_CC_RESERVED0G_OS						\
    _IOWR(QLMULTIPATH_MAGIC, 0x0e, sizeof(EXT_IOCTL))
#define EXT_CC_RESERVED0H_OS						\
    _IOWR(QLMULTIPATH_MAGIC, 0x0f, sizeof(EXT_IOCTL))

#define EXT_CC_RESERVED0I_OS						\
    _IOWR(QLMULTIPATH_MAGIC, 0x10, sizeof(EXT_IOCTL))
#define EXT_CC_RESERVED0J_OS						\
    _IOWR(QLMULTIPATH_MAGIC, 0x11, sizeof(EXT_IOCTL))

#define EXT_CC_RESERVED0Z_OS						\
    _IOWR(QLMULTIPATH_MAGIC, 0x21, sizeof(EXT_IOCTL))


/*
 * These are Linux driver implementation specific commands. Values are
 * in decreasing order.
 */

#define EXT_CC_STARTIOCTL				/* STARTIOCTL */ \
    _IOWR(QLMULTIPATH_MAGIC, 0xff, sizeof(EXT_IOCTL))
#define EXT_CC_SETINSTANCE				/* SETINSTANCE */ \
    _IOWR(QLMULTIPATH_MAGIC, 0xfe, sizeof(EXT_IOCTL))
#define	EXT_CC_WWPN_TO_SCSIADDR				/* WWPN_TO_SCSIADDR */ \
    _IOWR(QLMULTIPATH_MAGIC, 0xfd, sizeof(EXT_IOCTL))





 

/*
 * Overrides for Emacs so that we almost follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 2
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -2
 * c-argdecl-indent: 2
 * c-label-offset: -2
 * c-continued-statement-offset: 2
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */

#endif /* _EXIOCT_LN_H_ */

