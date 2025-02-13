//////////////////////////////////////////////////////////////////////////////
// This file is part of 'fcomUtil'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'fcomUtil', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
/*=============================================================================

  Name: drvFcom.h

  Abs:  function prototypes, defines for drvFcom.c

  Auth: 02 JUN-2016, bhill
  Rev:  

-----------------------------------------------------------------------------*/
#include "copyright_SLAC.h"
#include <dbScan.h>
#include <epicsMutex.h>
#include <fcom_api.h>

#ifndef INCdrvFcomH
#define INCdrvFcomH

#ifdef __cplusplus
extern "C" {
#endif 	/* __cplusplus */



#ifdef __cplusplus
}
#endif 	/* __cplusplus */

/*
 * Limits for blobs, callbacks, groups, and sets
 * Can be increased if needed
 */
#define	N_DRV_FCOM_BLOBS_MAX	50
#define	N_DRV_FCOM_CB_MAX		50
#define	N_DRV_FCOM_GROUPS_MAX	20
#define	N_DRV_FCOM_SETS_MAX		20

/**
 ** Initialize a blob in a group
 **/
extern int	drvFcomInitGroupBlob( unsigned int	iGroup, FcomID	id, unsigned int fComType, const char * name );

/**
 ** Update a double value in a blob in a group
 **/
extern int	drvFcomUpdateGroupBlobDouble( unsigned int iGroup, FcomID id, unsigned int iValue, double value, epicsTimeStamp * pts );

/**
 ** Update an epicsUint32 value in a blob in a group
 **/
extern int	drvFcomUpdateGroupBlobLong( unsigned int iGroup, FcomID id, unsigned int iValue, epicsUInt32 value, epicsTimeStamp * pts );

/**
 ** Write the blob contents to it's Fcom group
 ** calls fcomAddGroup() internally
 ** Should only be called once per blob
 **/
extern int	drvFcomWriteBlobToGroup( unsigned int iGroup, FcomID id );

/**
 ** Send the Fcom blob group
 ** calls fcomPutGroup() internally
 **/
extern int	drvFcomPutGroup( unsigned int iGroup );

/**
 ** Return the IOSCANPVT pointer for the specified drvFcom group
 ** Returns NULL for an invalid group number.
 **/
IOSCANPVT	drvFcomGetGroupIoScan( unsigned int	iGroup );

/**
 ** Return the IOSCANPVT pointer for the specified drvFcom set
 ** Returns NULL for an invalid set number.
 **/
IOSCANPVT	drvFcomGetSetIoScan( unsigned int	iSet );

/**
 ** Add a blob to a set
 **/
extern int	drvFcomAddBlobToSet( unsigned int iSet, FcomID	id, const char * name );

extern void	drvFcomSetSyncEventCode( int eventCode );

extern void	drvFcomSignalSet( unsigned int	iSet );

typedef void (*SET_CB_FUNCPTR)( void * );
extern long drvFcomAddSetCallback( unsigned int iSet, SET_CB_FUNCPTR cb, void * pArg );

extern	epicsMutexId	drvFcomMutex;

#endif  /* INCdrvFcomH */
