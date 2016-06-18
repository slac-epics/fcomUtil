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

/**
 * Diagnostic function to support timing diagnostics relative to start of beam.
 * Set this to a function that can return the 64 bit cpu tsc for the fiducial event.
 */
typedef	unsigned long long	(*GET_BEAM_TSC)();
extern void	drvFcomSetFuncGetBeamStart( GET_BEAM_TSC pGetBeamStartFunc );

/**
 * Diagnostic function to support timing diagnostics
 * Set this to a function that can convert a duration in 64 bit cpu tsc to seconds
 */
typedef	double	(*TSC_TO_TICKS)( unsigned long long );
extern void	drvFcomSetFuncTicksToSec( TSC_TO_TICKS pTicksToSecFunc );

extern void	drvFcomSignalSet( unsigned int	iSet );

typedef void (*SET_CB_FUNCPTR)( void * );
extern long drvFcomAddSetCallback( unsigned int iSet, SET_CB_FUNCPTR cb, void * pArg );

extern	epicsMutexId	drvFcomMutex;

#endif  /* INCdrvFcomH */
