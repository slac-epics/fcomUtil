/*=============================================================================

  Name: drvFcom.c

  Abs:  FCOM driver for fcom blob send/receive via db files

  Auth: Bruce Hill (bhill)

  Mod:  ??-Jun-2016 - B.Hill - Initial Release

-----------------------------------------------------------------------------*/
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>

#if 0
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/uio.h>
#include <net/if.h>
#endif

#include <drvSup.h>
#include <cantProceed.h>
#include <epicsAssert.h>
#include <epicsEvent.h>
#include <epicsExit.h>
#include <epicsExport.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <epicsTypes.h>
#include <errlog.h>
#include <initHooks.h>
#include <registryFunction.h>

#include "fcom_api.h"
#include "fcomUtil.h"
#include "drvFcom.h"
#include "HiResTimeStub.h"
#if 0
#include <cadef.h>
#include <alarm.h>
#include "HiResTime.h"
#include "ContextTimer.h"
#include "evrTime.h"
#include "evrPattern.h"
#include "udpComm.h"
#include "fcomLclsBpm.h"
#include "fcomLclsBlen.h"
#include "fcomLclsLlrf.h"
#include "debugPrint.h"
extern int	fcomUtilFlag;
#include "devBusMapped.h"
#endif

#define FCOM_DRV_VERSION "FCOM driver $Revision: 1.2 $/$Name:  $"


#define TASK_PRIORITY   epicsThreadPriorityMax  /* Highest EPICS thread priority */

#define DEFAULT_SET_TIMEOUT		0.007		/* Default set  timeout */
#define DEFAULT_SYNC_TIMEOUT	0.1			/* Default sync timeout */

#ifndef PULSEID
#define	PULSEID_INVALID		(0x1FFFF)
#define PULSEID(time)		((time).nsec & PULSEID_INVALID)
#endif	/* PULSEID */

/* FcomID fcomBlobIDs[N_PULSE_BLOBS] = { 0 }; */

/* FcomBlobSetRef  fcomBlobSet = 0; */

typedef	struct	grpBlob
{
	FcomBlob		blob;		/* Fcom Blob to manage */
	size_t			nAlloc;		/* Number of values in currently allocated data block */
	const char	*	name;		/* Original blob name */
}	grpBlob;

typedef	struct	drvGroup
{
	FcomGroup		groupHandle;
	int				nBlobsAdded;
	IOSCANPVT		IoScan;
	grpBlob			txBlobs[N_DRV_FCOM_BLOBS_MAX];
}	drvGroup;

typedef	struct	setCallback
{
	SET_CB_FUNCPTR 	cb;
	void		* 	arg;
}	setCallback;

typedef	struct	setBlob
{
	unsigned int	nGoodBlobs;		/* Number of good receptions of this blob	*/
	unsigned int	nTimeouts;		/* Number of timeouts for this blob			*/
	unsigned int	nGetErrors;		/* Number of other fcomBlobGet errors		*/
	unsigned int	nStatErrors;	/* Number of blobs w/ non-zero stat			*/
	unsigned int	nTsErrors;		/* Number of blobs w/ mismatched timestamps	*/
	FcomID			blobId;			/* Fcom Blob ID */
	FcomBlob	*	pBlob;			/* Ptr to Fcom Blob */
	const char	*	name;			/* Original blob name */
}	setBlob;

typedef	struct	drvSet
{
	double			timeout;	/* Timeout for blob arrival after beam (sec) */
	epicsEventId	BlobSetSyncEvent;
	IOSCANPVT		IoScan;
	int				nBlobsAdded;
	int				exitTask;
	unsigned int	iSet;
	setBlob			rxBlobs[N_DRV_FCOM_BLOBS_MAX];
	setCallback		setCallbacks[N_DRV_FCOM_CB_MAX];
}	drvSet;

epicsMutexId		drvFcomMutex;
int					drvFcomSyncEventCode	= 0;
static	drvGroup	drvFcomGroups[ N_DRV_FCOM_GROUPS_MAX ];
static	drvSet		drvFcomSets[   N_DRV_FCOM_SETS_MAX   ];

int 	DEBUG_DRV_FCOM_RECV = 2;
int 	DEBUG_DRV_FCOM_SEND = 2;
epicsExportAddress(int, DEBUG_DRV_FCOM_RECV);
epicsExportAddress(int, DEBUG_DRV_FCOM_SEND);

/* Share with device support */

/*==========================================================*/

epicsTimeStamp  fcomFiducialTime;
t_HiResTime		fcomFiducialTsc	= 0LL;	/* 64 bit hi-res cpu timestamp counter */
int				fcomFiducial	= 0x1FFFF;

/*
 * Diagnostic timing support
 * When installed, it returns the CPU tsc for the start of the beam interval
 */
typedef	unsigned long long	(*GET_BEAM_TSC_FUNC)();
GET_BEAM_TSC_FUNC	GetBeamStartTsc	= NULL;	/* 64 bit hi-res cpu timestamp counter */
void	drvFcomSetFuncGetBeamStart( )
{
	REGISTRYFUNCTION	func = registryFunctionFind( "evrGetFiducialTsc" );
	if ( func )
		GetBeamStartTsc = (GET_BEAM_TSC_FUNC) func;
	else
		printf( "evrGetFiducialTsc function not available!\n" );
}

/*
 * To allow full diagnostic timing, call drvFcomSetFuncTicksToSec
 * w/ a function ptr to a routine that returns the cpu tsc for
 * the start of the beam interval
 */
typedef	double	(*TSC_TO_TICKS_FUNC)( unsigned long long );
TSC_TO_TICKS_FUNC	TicksToSec	= NULL;	/* 64 bit hi-res cpu timestamp counter */
void	drvFcomSetFuncTicksToSec( )
{
	REGISTRYFUNCTION	func = registryFunctionFind( "HiResTicksToSeconds" );
	if ( func )
		TicksToSec = (TSC_TO_TICKS_FUNC) func;
	else
		printf( "HiResTicksToSeconds function not available!\n" );
}

static epicsInt32
usSinceFiducial(void)
{
	t_HiResTime		curTick		= GetHiResTicks();
	epicsInt32		usSinceFid	= -1;
	if ( fcomFiducialTsc != 0 && TicksToSec != NULL )
	{
		usSinceFid = (*TicksToSec)( curTick - fcomFiducialTsc ) * 1e6;
	}
	return usSinceFid;
}


static int
pvTimePulseIdMatches(
	epicsTimeStamp	*	p_ref,
	FcomBlobRef			p_cmp	)
{
	epicsUInt32 idref, idcmp, diff;

	idref = PULSEID((*p_ref));
	idcmp = p_cmp->fc_tsLo & PULSEID_INVALID;

	if ( idref == idcmp && idref != PULSEID_INVALID ) {
		/* Verify that seconds match to less that a few minutes */
		diff = abs( p_ref->secPastEpoch - p_cmp->fc_tsHi );
		if ( diff < 4*60 )
			return 0;
	}

	return -1;
}


/* Internal use only.  Call w/ mutex locked */
static grpBlob	*	drvFcomFindAvailBlobForId( grpBlob * pFirstTxBlob, FcomID id  )
{
	size_t		iBlob;
	grpBlob	*	pGrpBlob	= pFirstTxBlob;
	for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pGrpBlob++ )
	{
		if( pGrpBlob->blob.fc_idnt == id )
			return pGrpBlob;
		if( pGrpBlob->blob.fc_idnt == 0 )
			return pGrpBlob;
	}
	return NULL;
}

/* Internal use only.  Call w/ mutex locked */
static grpBlob	*	drvFcomFindBlobForId( grpBlob * pFirstTxBlob, FcomID id  )
{
	size_t		iBlob;
	grpBlob	*	pGrpBlob	= pFirstTxBlob;
	for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pGrpBlob++ )
	{
		if( pGrpBlob->blob.fc_idnt == id )
			return pGrpBlob;
	}
	return NULL;
}

/* Internal use only.  Call w/ mutex locked */
setBlob	*	drvFcomFindAvailSetBlobForId( setBlob * pFirstRxBlob, FcomID id  )
{
	size_t		iBlob;
	setBlob	*	pSetBlob	= pFirstRxBlob;
	for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pSetBlob++ )
	{
		if( pSetBlob->blobId == id )
			return pSetBlob;
		if( pSetBlob->blobId == 0 )
			return pSetBlob;
	}
	return NULL;
}

/* Internal use only.  Call w/ mutex locked */
setBlob	*	drvFcomFindSetBlobForId( setBlob * pFirstRxBlob, FcomID id  )
{
	size_t		iBlob;
	setBlob	*	pSetBlob	= pFirstRxBlob;
	for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pSetBlob++ )
	{
		if( pSetBlob->blobId == id )
			return pSetBlob;
	}
	return NULL;
}

/**
 ** Initialize a blob in a group
 **/
int		drvFcomInitGroupBlob( unsigned int	iGroup, FcomID	id, unsigned int fComType, const char * name )
{
	if ( iGroup >= N_DRV_FCOM_GROUPS_MAX )
	{
		errlogPrintf( "drvFcomInitGroupBlob: Invalid group %d\n", iGroup );
		return -1;
	}

	epicsMutexLock( drvFcomMutex );
	drvGroup	*	pGroup = &drvFcomGroups[iGroup];
	if ( pGroup->IoScan == NULL )
	{
		/* Initialize group */
		scanIoInit( &pGroup->IoScan );
		pGroup->nBlobsAdded	= 0;
	}

	/* Find the blob for this id, or an available blob */
	grpBlob	*	pGrpBlob	= drvFcomFindAvailBlobForId( &pGroup->txBlobs[0], id );
	if ( pGrpBlob == NULL )
	{
		errlogPrintf( "drvFcomInitGroupBlob: Group %d, Blob "FCOM_ID_FMT": No more blobs available\n", iGroup, id );
		epicsMutexUnlock( drvFcomMutex );
		return -1;
	}

	if ( pGrpBlob->blob.fc_idnt == 0 )
	{
		/* Initialize drvFcom blob for up to N values */
		pGrpBlob->nAlloc		= 8;	/* Start w/ 8, can be increased as needed */
		pGrpBlob->name			= name;
		pGrpBlob->blob.fc_raw	= calloc( pGrpBlob->nAlloc, FCOM_EL_SIZE(fComType) );
		pGrpBlob->blob.fc_vers	= FCOM_PROTO_VERSION;
		pGrpBlob->blob.fc_idnt	= id;
		pGrpBlob->blob.fc_type	= fComType;
		pGrpBlob->blob.fc_stat	= 0;
		pGrpBlob->blob.fc_tsLo	= 0;
		pGrpBlob->blob.fc_tsHi	= 0;
		pGrpBlob->blob.fc_nelm	= 0;

		if ( DEBUG_DRV_FCOM_SEND >= 1 )
			printf( "drvFcomInitGroupBlob: Group %u, Blob "FCOM_ID_FMT", %s\n", iGroup, id, name );
	}
	else
	{
		if ( DEBUG_DRV_FCOM_SEND >= 2 )
			printf( "drvFcomInitGroupBlob: Group %u, Blob "FCOM_ID_FMT", %s already initialized.\n", iGroup, id, name );
	}

	epicsMutexUnlock( drvFcomMutex );
	return(0);
}

/**
 ** Update a double value in a blob in a group
 **/
int		drvFcomUpdateGroupBlobDouble(
	unsigned int		iGroup,
	FcomID				id,
	unsigned int		iValue,
	double				value,
	epicsTimeStamp	*	pTimeStamp )
{
	int					fid	= 0x1FFFF;

	if ( DEBUG_DRV_FCOM_SEND >= 5 )
		printf( "drvFcomUpdateGroupBlobDouble Entry: Group %u, Blob "FCOM_ID_FMT", Value %f\n", iGroup, id, value );
	if ( iGroup >= N_DRV_FCOM_GROUPS_MAX )
	{
		errlogPrintf( "drvFcomUpdateGroupBlobDouble: Invalid group %d\n", iGroup );
		return FCOM_ERR_INVALID_ARG;
	}

	epicsMutexLock( drvFcomMutex );
	drvGroup	*	pGroup = &drvFcomGroups[iGroup];

	/* Find the blob for this id */
	grpBlob	*	pGrpBlob	= drvFcomFindBlobForId( &pGroup->txBlobs[0], id );
	if ( pGrpBlob == NULL )
	{
		errlogPrintf( "drvFcomUpdateGroupBlobDouble: Group %d, Blob "FCOM_ID_FMT" not found!\n", iGroup, id );
		epicsMutexUnlock( drvFcomMutex );
		return FCOM_ERR_ID_NOT_FOUND;
	}

	if( pGrpBlob->nAlloc <= iValue )
	{
		pGrpBlob->nAlloc *= 2;	/* Double the capacity */
		if ( DEBUG_DRV_FCOM_SEND >= 2 )
			printf( "drvFcomUpdateGroupBlobDouble: Group %u, Blob "FCOM_ID_FMT", bumping capacity to %zu\n", iGroup, id, pGrpBlob->nAlloc );
		if( pGrpBlob->nAlloc <= iValue )
		{
			if ( iValue > 1000 )
			{	/* Probably an error */
				errlogPrintf( "drvFcomUpdateGroupBlobDouble: Group %d, Blob "FCOM_ID_FMT", Value index %d too large!\n", iGroup, id, iValue );
				epicsMutexUnlock( drvFcomMutex );
				return -1;
			}
			pGrpBlob->nAlloc = iValue;
		}
		pGrpBlob->blob.fc_raw	= realloc( pGrpBlob->blob.fc_raw, pGrpBlob->nAlloc * FCOM_EL_SIZE(pGrpBlob->blob.fc_type) );
		if ( pGrpBlob->blob.fc_raw == NULL )
		{
			pGrpBlob->nAlloc = 0;
			errlogPrintf( "drvFcomUpdateGroupBlobDouble: Group %d, Blob "FCOM_ID_FMT", Unable to realloc for Value index %d!\n", iGroup, id, iValue );
			epicsMutexUnlock( drvFcomMutex );
			return -1;
		}
	}

	if( pGrpBlob->blob.fc_nelm < (iValue + 1) )
		pGrpBlob->blob.fc_nelm = (iValue + 1);
	if( pGrpBlob->blob.fc_type == FCOM_EL_DOUBLE )
		pGrpBlob->blob.fc_dbl[iValue] = value;
	else
		pGrpBlob->blob.fc_flt[iValue] = value;
	if ( pTimeStamp != NULL )
	{
		pGrpBlob->blob.fc_tsHi = pTimeStamp->secPastEpoch;
		pGrpBlob->blob.fc_tsLo = pTimeStamp->nsec;
		fid = pGrpBlob->blob.fc_tsLo & 0x1FFFF;
	}

	epicsMutexUnlock( drvFcomMutex );

	if ( DEBUG_DRV_FCOM_SEND >= 4 )
		printf( "drvFcomUpdateGroupBlobDouble: Group %u, Blob "FCOM_ID_FMT", %s, val[%d]=%f, fid %d\n",
				iGroup, id, pGrpBlob->name, iValue, value, fid );

	return(0);
}

/**
 ** Update a uint32 value in a blob in a group
 **/
int		drvFcomUpdateGroupBlobLong(
	unsigned int		iGroup,
	FcomID				id,
	unsigned int		iValue,
	epicsUInt32			value,
	epicsTimeStamp	*	pTimeStamp )
{
	if ( iGroup >= N_DRV_FCOM_GROUPS_MAX )
	{
		errlogPrintf( "drvFcomUpdateGroupBlobLong: Invalid group %d\n", iGroup );
		return -1;
	}

	epicsMutexLock( drvFcomMutex );
	drvGroup	*	pGroup = &drvFcomGroups[iGroup];

	/* Find the blob for this id */
	grpBlob	*	pGrpBlob	= drvFcomFindBlobForId( &pGroup->txBlobs[0], id );
	if ( pGrpBlob == NULL )
	{
		errlogPrintf( "drvFcomUpdateGroupBlobLong: Group %d, Blob "FCOM_ID_FMT" not found!\n", iGroup, id );
		epicsMutexUnlock( drvFcomMutex );
		return FCOM_ERR_ID_NOT_FOUND;
	}

	if( pGrpBlob->nAlloc <= iValue )
	{
		pGrpBlob->nAlloc *= 2;	/* Double the capacity */
		if( pGrpBlob->nAlloc <= iValue )
		{
			if ( iValue > 1000 )
			{	/* Probably an error */
				errlogPrintf( "drvFcomUpdateGroupBlobLong: Group %d, Blob "FCOM_ID_FMT", Value index %d too large!\n", iGroup, id, iValue );
				epicsMutexUnlock( drvFcomMutex );
				return FCOM_ERR_INVALID_ARG;
			}
			pGrpBlob->nAlloc = iValue;
		}
		pGrpBlob->blob.fc_raw	= realloc( pGrpBlob->blob.fc_raw, pGrpBlob->nAlloc * FCOM_EL_SIZE(pGrpBlob->blob.fc_type) );
		if ( pGrpBlob->blob.fc_raw == NULL )
		{
			pGrpBlob->nAlloc = 0;
			errlogPrintf( "drvFcomUpdateGroupBlobLong: Group %d, Blob "FCOM_ID_FMT", Unable to realloc for Value index %d!\n", iGroup, id, iValue );
			epicsMutexUnlock( drvFcomMutex );
			return FCOM_ERR_NO_MEMORY;
		}
	}

	if( pGrpBlob->blob.fc_nelm < iValue )
		pGrpBlob->blob.fc_nelm = iValue;
	pGrpBlob->blob.fc_u32[iValue] = value;
	if ( pTimeStamp != NULL )
	{
		pGrpBlob->blob.fc_tsHi = pTimeStamp->secPastEpoch;
		pGrpBlob->blob.fc_tsLo = pTimeStamp->nsec;
	}

	epicsMutexUnlock( drvFcomMutex );
	return(0);
}

/**
 ** Write the blob contents to it's Fcom group
 ** calls fcomAddGroup() internally
 ** Should only be called once per blob
 **/
int		drvFcomWriteBlobToGroup( unsigned int iGroup, FcomID id )
{
	int	fid;
	int	status	= 0;

	if ( DEBUG_DRV_FCOM_SEND >= 5 )
		printf( "drvFcomWriteBlobToGroup Entry: Group %u, Blob "FCOM_ID_FMT"\n", iGroup, id );
	if ( iGroup >= N_DRV_FCOM_GROUPS_MAX )
	{
		errlogPrintf( "drvFcomWriteBlobToGroup: Invalid group %d\n", iGroup );
		return FCOM_ERR_INVALID_ARG;
	}

	epicsMutexLock( drvFcomMutex );
	drvGroup	*	pGroup = &drvFcomGroups[iGroup];
	if ( pGroup->IoScan == NULL )
	{
		errlogPrintf( "drvFcomWriteBlobToGroup: Group %d not initialized!\n", iGroup );
		epicsMutexUnlock( drvFcomMutex );
		return FCOM_ERR_INVALID_ARG;
	}
	if ( pGroup->groupHandle == NULL )
	{
		/* First blob for this group so allocate an fcom group */
		status = fcomAllocGroup( id, &pGroup->groupHandle );
		if ( status != 0 || pGroup->groupHandle == NULL )
		{
			errlogPrintf( "drvFcomWriteBlobToGroup Error: Group %d, Blob "FCOM_ID_FMT", %s\n", iGroup, id, fcomStrerror(status) );
			epicsMutexUnlock( drvFcomMutex );
			return status;
		}
	}

	/* Find the blob for this id */
	grpBlob	*	pGrpBlob	= drvFcomFindBlobForId( &pGroup->txBlobs[0], id );
	if ( pGrpBlob == NULL )
	{
		errlogPrintf( "drvFcomWriteBlobToGroup: Group %d, Blob "FCOM_ID_FMT" not found!\n", iGroup, id );
		epicsMutexUnlock( drvFcomMutex );
		return FCOM_ERR_ID_NOT_FOUND;
	}
	if ( pGrpBlob->blob.fc_raw == NULL )
	{
		errlogPrintf( "drvFcomWriteBlobToGroup: Group %d, Blob "FCOM_ID_FMT", data allocation error!\n", iGroup, id );
		epicsMutexUnlock( drvFcomMutex );
		return -1;
	}
	fid = pGrpBlob->blob.fc_tsLo & 0x1FFFF;

	/* Add the blob to the group */
	status = fcomAddGroup( pGroup->groupHandle, &pGrpBlob->blob );
	if ( status != 0 )
	{
		errlogPrintf( "drvFcomWriteBlobToGroup Error: Group %d, Blob "FCOM_ID_FMT", %s\n", iGroup, id, fcomStrerror(status) );
		epicsMutexUnlock( drvFcomMutex );
		return status;
	}
	pGroup->nBlobsAdded++;

	epicsMutexUnlock( drvFcomMutex );

	if ( DEBUG_DRV_FCOM_SEND >= 3 )
		printf( "drvFcomWriteBlobToGroup: Group %u, Blob "FCOM_ID_FMT", %s, fid %d\n", iGroup, id, pGrpBlob->name, fid );

	return(0);
}

/**
 ** Send the Fcom blob group
 ** calls fcomPutGroup() internally
 **/
int		drvFcomPutGroup( unsigned int iGroup )
{
	int	status	= 0;

	if ( iGroup >= N_DRV_FCOM_GROUPS_MAX )
	{
		errlogPrintf( "drvFcomPutGroup: Invalid group %d\n", iGroup );
		return FCOM_ERR_INVALID_ARG;
	}

	if ( DEBUG_DRV_FCOM_SEND >= 3 )
		printf( "drvFcomPutGroup: Sending Group %u\n", iGroup );

	epicsMutexLock( drvFcomMutex );
	drvGroup	*	pGroup = &drvFcomGroups[iGroup];
	if ( pGroup->groupHandle == NULL )
	{
		errlogPrintf( "drvFcomPutGroup: Group %d not initialized!\n", iGroup );
		epicsMutexUnlock( drvFcomMutex );
		return FCOM_ERR_NO_DATA;
	}
	if ( pGroup->nBlobsAdded == 0 )
	{
		errlogPrintf( "drvFcomPutGroup: Group %d, no blobs added!\n", iGroup );
		fcomFreeGroup( pGroup->groupHandle );
		pGroup->groupHandle = NULL;
		epicsMutexUnlock( drvFcomMutex );
		return FCOM_ERR_NO_DATA;
	}

	/* Send the group */
	status = fcomPutGroup( pGroup->groupHandle );
	
	/* fcom will free the group once sent.
	 * Clear the groupHandle and nBlobsAdded.
	 */
	pGroup->groupHandle = NULL;
	pGroup->nBlobsAdded = 0;
	if ( status != 0 )
	{
		errlogPrintf( "drvFcomPutGroup Error: Group %d, %s!\n", iGroup, fcomStrerror(status) );
		epicsMutexUnlock( drvFcomMutex );
		return status;
	}

	/* Initiate scanIo processing for this group's I/O Intr records */
	scanIoRequest( pGroup->IoScan );

	epicsMutexUnlock( drvFcomMutex );

	if ( DEBUG_DRV_FCOM_SEND >= 5 )
		printf( "drvFcomPutGroup Exit: Group %u\n", iGroup );
	return(0);
}

/**
 ** Returns ptr to ioscan for the specified group
 **/
IOSCANPVT	drvFcomGetGroupIoScan( unsigned int iGroup )
{
	if ( iGroup >= N_DRV_FCOM_GROUPS_MAX )
	{
		errlogPrintf( "drvFcomGetGroupIoScan: Invalid group %d\n", iGroup );
		return NULL;
	}

	epicsMutexLock( drvFcomMutex );
	drvGroup	*	pGroup = &drvFcomGroups[iGroup];
	if ( pGroup->IoScan == NULL )
	{
		errlogPrintf( "drvFcomGetGroupIoScan: Group %d not initialized!\n", iGroup );
		return NULL;
	}

	return( pGroup->IoScan );
}

/*
 *	FCOM Set support
 */

/**
 ** Returns ptr to ioscan for the specified Set
 **/
IOSCANPVT	drvFcomGetSetIoScan( unsigned int iSet )
{
	if ( iSet >= N_DRV_FCOM_SETS_MAX )
	{
		errlogPrintf( "drvFcomGetSetIoScan: Invalid set %d\n", iSet );
		return NULL;
	}

	epicsMutexLock( drvFcomMutex );
	drvSet	*	pSet = &drvFcomSets[iSet];
	if ( pSet->IoScan == NULL )
	{
		errlogPrintf( "drvFcomGetSetIoScan: Set %d not initialized!\n", iSet );
	}

	epicsMutexUnlock( drvFcomMutex );
	return( pSet->IoScan );
}

long drvFcomAddSetCallback( unsigned int iSet, SET_CB_FUNCPTR cb, void * pArg )
{
	unsigned int	iCb;
	drvSet		*	pSet;

	if ( iSet >= N_DRV_FCOM_SETS_MAX )
	{
		errlogPrintf( "drvFcomAddSetCallback: Invalid set %d\n", iSet );
		return -1;
	}

	pSet = &drvFcomSets[iSet];
	for ( iCb = 0; iCb < N_DRV_FCOM_CB_MAX; iCb++ )
	{
		if ( pSet->setCallbacks[iCb].cb == NULL )
		{
			pSet->setCallbacks[iCb].cb	= cb;
			pSet->setCallbacks[iCb].arg	= pArg;
			if ( DEBUG_DRV_FCOM_RECV >= 1 )
				printf( "drvFcomAddSetCallback: Added callback for set %u\n", iSet );
			return 0;
		}
	}

	errlogPrintf( "drvFcomAddSetCallback: Set %d, No more callbacks available!\n", iSet );
	return -1;
}

int		DEBUG_DRV_FCOM_COUNTDOWN	= 0;

/* This function is called at start of beam w/o from
 * a bo record for each set w/ SCAN=Event and EVNT=40 (or other EC as needed)
 */
void drvFcomSignalSet( unsigned int	iSet )
{
	epicsTimeStamp		ts;
	int					status;

	if ( GetBeamStartTsc )
		fcomFiducialTsc = GetBeamStartTsc();
	status = epicsTimeGetEvent( &ts, 1 );
	if ( status == 0 )
		fcomFiducial = ts.nsec & 0x1FFFF;
	else
		fcomFiducial = 0x1FFFF;

/* HACK */
	if ( DEBUG_DRV_FCOM_RECV >= 4 && DEBUG_DRV_FCOM_COUNTDOWN == 0 )
	{
		DEBUG_DRV_FCOM_SEND			= 3;
		DEBUG_DRV_FCOM_COUNTDOWN	= 10;
	}
	if ( DEBUG_DRV_FCOM_COUNTDOWN > 0 )
	{
		--DEBUG_DRV_FCOM_COUNTDOWN;
		if ( DEBUG_DRV_FCOM_COUNTDOWN == 0 )
		{
			DEBUG_DRV_FCOM_SEND		= 2;
			DEBUG_DRV_FCOM_RECV		= 2;
		}
	}

#if 0
	int					signalSetFid;
	int					fidLast40;
	unsigned long long	tscLast40;
	epicsTimeGetEvent( &ts, 40 );
	signalSetFid = ts.nsec & 0x1FFFF;
extern int evrGetLastFiducial40();
extern unsigned long long evrGetFiducialTsc40();
	fidLast40 = evrGetLastFiducial40();
	tscLast40 = evrGetFiducialTsc40();
	if ( DEBUG_DRV_FCOM_RECV >= 3 ) errlogPrintf( "drvFcomSignalSet: set %d, EC40fid %d, fidLast40 %d, lastfid=%d (beam+%d)\n", iSet, signalSetFid, fidLast40, fcomFiducial, fcomFiducial%3 );
#endif

	if ( iSet >= N_DRV_FCOM_SETS_MAX )
	{
		errlogPrintf( "drvFcomGetSetIoScan: Invalid set %d\n", iSet );
		return;
	}

	if ( DEBUG_DRV_FCOM_RECV >= 5 ) errlogPrintf( "drvFcomSignalSet: Signaling set %d, %d us after fiducial %d.\n", iSet, usSinceFiducial(), fcomFiducial );
	if ( drvFcomSets[iSet].BlobSetSyncEvent != NULL )
	{
		/* Signal the BlobSetSyncEvent to trigger the fcomGetBlobSet call */
		epicsEventSignal( drvFcomSets[iSet].BlobSetSyncEvent );
	}
	return;
}

static int drvFcomSetTask(void * parg)
{
	int					fid;
	unsigned int		iBlob;
	unsigned int		iCb;
	int					status	= 0;
	drvSet			*	pSet	= (drvSet *) parg;
/*	unsigned long long	tscNow; */
	epicsTimeStamp		syncTimeStamp;
	epicsInt32			syncWakeupDelay;
	double				syncTimeout	= DEFAULT_SYNC_TIMEOUT;

    epicsMutexLock(drvFcomMutex);
	/* Create synchronization event */
	pSet->BlobSetSyncEvent = epicsEventMustCreate( epicsEventEmpty );
		
	/* Register EVRFire */
	/* evrTimeRegister(EVRFire, fcomBlobSet); */
    epicsMutexUnlock(drvFcomMutex);

	if( pSet->timeout == 0 )
		pSet->timeout = DEFAULT_SET_TIMEOUT;

	while ( pSet->exitTask == 0 )
	{
		status = epicsEventWaitWithTimeout( pSet->BlobSetSyncEvent, syncTimeout );
		switch( status )
		{
		case epicsEventWaitTimeout:
			if ( DEBUG_DRV_FCOM_RECV >= 3 ) errlogPrintf( "drvFcomSetTask %d: %f sec timeout waiting for Sync event\n", pSet->iSet, syncTimeout );
			continue;
		default:
			if ( DEBUG_DRV_FCOM_RECV >= 1 )
				errlogPrintf( "Unexpected epicsEventWaitWithTimeout Error: %d\n", status );
			continue;
		case epicsEventWaitOK:
			break;
		}

		syncWakeupDelay	= usSinceFiducial();
		if ( /* syncWakeupDelay >= 0 && */ DEBUG_DRV_FCOM_RECV >= 4 )
			errlogPrintf("drvFcomSetTask %d: Woke up %d us after fiducial %d.\n", pSet->iSet, syncWakeupDelay, fcomFiducial );

		if ( drvFcomSyncEventCode ) {
			status = epicsTimeGetEvent( &syncTimeStamp, drvFcomSyncEventCode );
		}

#if 0	/* fcomGetBlobSet */
		{
			int		getBlobTime	= usSinceFiducial();
			if(DEBUG_DRV_FCOM_RECV >= 4) errlogPrintf("Calling fcomGetBlobSet w/ timeout %u ms at FID+%d us\n", (unsigned int) timer_delay_ms, getBlobTime );
			t_HiResTime		tickBeforeGet	= GetHiResTicks();
			/* Get all the blobs w/ timeout timer_delay_ms */
			FcomBlobSetMask	waitFor	= (1<<N_PULSE_BLOBS) - 1;
			status = fcomGetBlobSet( fcomBlobSet, &got_mask, waitFor, FCOM_SET_WAIT_ALL, timer_delay_ms );
			t_HiResTime		tickAfterGet	= GetHiResTicks();
			double			usInFCom		= HiResTicksToSeconds( tickAfterGet - tickBeforeGet ) * 1e6;
			if ( status && FCOM_ERR_TIMEDOUT != status )
			{
				errlogPrintf("fcomGetBlobSet failed: %s; sleeping for 2 seconds\n", fcomStrerror(status));
				for ( loop = 0; loop < N_PULSE_BLOBS; loop++ )
					fcomFcomGetErrs[loop]++;
				epicsThreadSleep( 2.0 );
				continue;
			}
			if ( status == FCOM_ERR_TIMEDOUT )
			{
				/* If a timeout happened then fall through; there still might be good * blobs...  */
				if(DEBUG_DRV_FCOM_RECV >= 3) errlogPrintf("fcomGetBlobSet %u ms timeout in %.1fus! req 0x%X, got 0x%X\n", (unsigned int) timer_delay_ms, usInFCom, (unsigned int)waitFor, (unsigned int)got_mask );
			}
			else
			{
				if(DEBUG_DRV_FCOM_RECV >= 4) errlogPrintf("fcomGetBlobSet succeeeded.\n");
			}
		}
		this_time = usSinceFiducial();

		if ( this_time > fcomMaxFcomDelayUs )
			fcomMaxFcomDelayUs = this_time;
		if ( this_time < fcomMinFcomDelayUs )
			fcomMinFcomDelayUs = this_time;

		/* Running average: fn+1 = 127/128 * fn + 1/128 * x = fn - 1/128 fn + 1/128 x */
		fcomAvgFcomDelayUs +=  ((int)(- fcomAvgFcomDelayUs + this_time)) >> 8;
#else	/* Not using fcomGetBlobSet */
		epicsMutexLock(drvFcomMutex);
		setBlob		*	pSetBlob	= &(pSet->rxBlobs[0]);
		for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pSetBlob++ )
		{
			if ( pSetBlob->pBlob != NULL )
				fcomReleaseBlob( &pSetBlob->pBlob );
			pSetBlob->pBlob	= NULL;
		}
		epicsMutexUnlock(drvFcomMutex);

		/* Sleep for the specified set timeout interval */
		struct timespec		blobTimeout;
		blobTimeout.tv_sec	= (int) floor( pSet->timeout );
		blobTimeout.tv_nsec	= (int) (( pSet->timeout - blobTimeout.tv_sec ) / 1e9 );
		nanosleep( &blobTimeout, 0 );

		/* Collect the blobs that arrived */
		epicsMutexLock(drvFcomMutex);
		pSetBlob	= &(pSet->rxBlobs[0]);
		for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pSetBlob++ )
		{
			if ( pSetBlob->blobId == FCOM_ID_NONE )
				continue;
			status = fcomGetBlob( pSetBlob->blobId, &pSetBlob->pBlob, 0 );

			switch ( status )
			{
			case 0:
				pSetBlob->nGoodBlobs++;
				break;
			case FCOM_ERR_NO_DATA:
				pSetBlob->nTimeouts++;
				continue;
			default:
				pSetBlob->nGetErrors++;
				errlogPrintf( "drvFcomSetTask Error: Set %d, Blob "FCOM_ID_FMT", %s\n", pSet->iSet, pSetBlob->blobId, fcomStrerror(status) );
				continue;
			}

			assert( pSetBlob->pBlob != NULL );
			/* Check blob status */
			if ( pSetBlob->pBlob->fc_stat != 0 )
			{
				/* TODO: Add checks for special cases
				 * FC_STAT_BPM_REFLO for BPMS
				 * FC_STAT_BLEN_INVAL_BIMASK for BLEN
				 */
				pSetBlob->nStatErrors++;
			}

			if ( drvFcomSyncEventCode ) {
				if ( status == 0 ) {
					status = pvTimePulseIdMatches( &syncTimeStamp, pSetBlob->pBlob );
					if ( status != 0 ) {
						pSetBlob->nTsErrors++;
					}
				}
			}
			fid = pSetBlob->pBlob->fc_tsLo & 0x1FFFF;
			if ( DEBUG_DRV_FCOM_RECV >= 3 )
				printf( "drvFcomSetTask: Set %u, Rcvd Blob "FCOM_ID_FMT", %s, fid %d, %d us after fid %d\n", pSet->iSet, pSetBlob->blobId, pSetBlob->name, fid, usSinceFiducial(), fcomFiducial );

		}
		epicsMutexUnlock(drvFcomMutex);
#endif 	/* fcomGetBlobSet */

		/*
		 * This scanIoRequest triggers I/O Scan processing of all the FCOM DTYP records.
		 * It's getting triggered after the FCOM blob's are fetched
		 */
		scanIoRequest( pSet->IoScan );

		/* Call the callbacks */
		for ( iCb = 0; iCb < N_DRV_FCOM_CB_MAX; iCb++ )
		{
			if( pSet->setCallbacks[iCb].cb != NULL )
			{
				if ( DEBUG_DRV_FCOM_RECV >= 4 )
					printf( "drvFcomAddSetCallback: Calling set %u callback %u, %d us after fid %d\n", pSet->iSet, iCb, usSinceFiducial(), fcomFiducial );
				pSet->setCallbacks[iCb].cb( pSet->setCallbacks[iCb].arg );
			}
		}
#if 0
		this_time = usSinceFiducial();
		if ( this_time > fcomMaxPostDelayUs )
			fcomMaxPostDelayUs = this_time;
		/* Running average: fn+1 = 127/128 * fn + 1/128 * x = fn - 1/128 fn + 1/128 x */
		fcomAvgPostDelayUs += ((int) (- fcomAvgPostDelayUs + this_time)) >> 8;
#endif
	}

	return(0);
}

#if 0
{
	int					status;
	epicsTimeStamp	*	p_refTime;
	FcomBlobSetMask		got_mask;
	epicsUInt32			this_time;

		if ( ! fcomBlobSet )
		{
			for ( loop = 0; loop < N_PULSE_BLOBS; loop++ ) {
				if ( fcomPulseBlobs[loop].blob ) {
					status = fcomReleaseBlob( & fcomPulseBlobs[loop].blob );
					if ( status ) {
						errlogPrintf("Fatal Error: Unable to release blob (for %s): %s\n",
								fcomPulseBlobs[loop].name,
								fcomStrerror(status));
						return -1;
					}
				}
			}
		}
	if ( fcomBlobSet ) {
		status = fcomFreeBlobSet( fcomBlobSet );
		if ( status )
			fprintf(stderr, "Unable to destroy blob set: %s\n", fcomStrerror(status));
		fcomBlobSet = 0;
	}

	for ( loop = 0; loop < N_PULSE_BLOBS; loop++ ) {
		status = fcomUnsubscribe( fcomBlobIDs[loop] );
		if ( status )
			fprintf(stderr, "Unable to unsubscribe %s from FCOM: %s\n", fcomPulseBlobs[loop].name, fcomStrerror(status));
	}
}
#endif

int drvFcomAddBlobToSet( unsigned int iSet, FcomID blobId, const char * name )
{
	int		status;

	if ( iSet >= N_DRV_FCOM_SETS_MAX )
	{
		errlogPrintf( "drvFcomAddBlobToSet: Invalid set %d\n", iSet );
		return -1;
	}

	epicsMutexLock( drvFcomMutex );
	drvSet	*	pSet = &drvFcomSets[iSet];
	if ( pSet->IoScan == NULL )
	{
		/* Initialize set */
		scanIoInit( &pSet->IoScan );
		pSet->nBlobsAdded	= 0;

		epicsThreadMustCreate( "drvFcom", TASK_PRIORITY, 20480, (EPICSTHREADFUNC) drvFcomSetTask, pSet );

#if 0
		if ( (status = fcomAllocBlobSet( fcomBlobIDs, sizeof(fcomBlobIDs)/sizeof(fcomBlobIDs[0]), &fcomBlobSet)) ) {
			errlogPrintf("ERROR: Unable to allocate blob set: %s; trying asynchronous mode\n", fcomStrerror(status));
			fcomBlobSet = 0;
		}
#endif
	}

	/* Find the blob for this id, or an available blob */
	setBlob	*	pSetBlob	= drvFcomFindAvailSetBlobForId( &pSet->rxBlobs[0], blobId );
	if ( pSetBlob == NULL )
	{
		errlogPrintf( "drvFcomAddBlobToSet: Set %d, Blob " FCOM_ID_FMT ": No more blobs available\n", iSet, blobId );
		epicsMutexUnlock( drvFcomMutex );
		return -1;
	}

	if ( pSetBlob->blobId == 0 )
	{
		/* Reserve this setBlob */
		pSetBlob->name		= name;
		pSetBlob->blobId	= blobId;

		/* fcomUtilFlag = DP_DEBUG; */
		/* No need to use FCOM_SYNC_GET unless we plan to call fcomGetBlob w/ a timeout */
		status = fcomSubscribe( blobId, FCOM_ASYNC_GET );
		if ( 0 != status )
		{
			pSetBlob->name		= "";
			pSetBlob->blobId	= FCOM_ID_NONE;
			errlogPrintf(	"FATAL ERROR: Unable to subscribe %s (" FCOM_ID_FMT ") to FCOM: %s\n",
							name, blobId, fcomStrerror(status) );
			epicsMutexUnlock( drvFcomMutex );
			return -1;
		}

		pSet->nBlobsAdded++;

		if ( DEBUG_DRV_FCOM_RECV >= 1 )
			printf( "drvFcomAddBlobToSet: Set %u, Subscribed to Blob "FCOM_ID_FMT", %s\n", iSet, blobId, name );
	}
	else
	{
		if ( DEBUG_DRV_FCOM_RECV >= 2 )
			printf( "drvFcomAddBlobToSet: Set %u, Blob "FCOM_ID_FMT", %s already subscribed.\n", iSet, blobId, name );
	}

	epicsMutexUnlock( drvFcomMutex );

	return 0;
}

void	drvFcomSetSyncEventCode( int eventCode )
{
	drvFcomSyncEventCode	= eventCode;
}

/**************************************************************************************************/
/* Here we supply the driver functions for epics                                            */
/**************************************************************************************************/

/* Driver Shutdown function */
static void drvFcomShutdown(void * parg)
{
    printf("drvFcomShutdown\n");
    epicsMutexLock(drvFcomMutex);
	/* TODO: Cleanup fcom groups, blobs, and sets */
    epicsMutexUnlock(drvFcomMutex);

    return;
}

/* Driver Initialization function */
static long drvFcom_Init()
{
	unsigned int iGroup;
	unsigned int iSet;
	unsigned int iCb;

	/* See if we have timing diagnostic functions available */
	drvFcomSetFuncGetBeamStart( );
	drvFcomSetFuncTicksToSec( );

    printf( "drvFcom_Init:\n" );

	drvFcomMutex = epicsMutexMustCreate();

	epicsMutexLock( drvFcomMutex );

	/* Initialize the driver tables */
	for ( iGroup = 0; iGroup < N_DRV_FCOM_GROUPS_MAX; iGroup++ )
	{
		size_t			iBlob;
		grpBlob		*	pGrpBlob;
		drvFcomGroups[iGroup].groupHandle	= NULL;
		drvFcomGroups[iGroup].IoScan		= NULL;
		pGrpBlob	= &(drvFcomGroups[iGroup].txBlobs[0]);
		for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pGrpBlob++ )
		{
			pGrpBlob->blob.fc_idnt	= 0;
			pGrpBlob->blob.fc_raw	= 0;
			pGrpBlob->name			= "";
		}
	}
	for ( iSet = 0; iSet < N_DRV_FCOM_SETS_MAX; iSet++ )
	{
		size_t			iBlob;
		setBlob		*	pSetBlob;
		drvSet		*	pSet = &drvFcomSets[iSet];
		pSet->IoScan		= NULL;
		pSet->exitTask		= 0;
		pSet->iSet			= iSet;
		pSetBlob	= &(pSet->rxBlobs[0]);
		for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pSetBlob++ )
		{
			pSetBlob->blobId		= 0;
			pSetBlob->pBlob			= NULL;
			pSetBlob->name			= "";
			pSetBlob->nGoodBlobs	= 0;
			pSetBlob->nTimeouts		= 0;
			pSetBlob->nGetErrors	= 0;
			pSetBlob->nStatErrors	= 0;
			pSetBlob->nTsErrors		= 0;
		}
		for ( iCb = 0; iCb < N_DRV_FCOM_CB_MAX; iCb++ )
		{
			pSet->setCallbacks[iCb].cb	= NULL;
			pSet->setCallbacks[iCb].arg	= NULL;
		}
	}
	epicsMutexUnlock( drvFcomMutex );

    epicsAtExit(drvFcomShutdown, NULL);

    return 0;
}

static long drvFcom_Report(int level)
{
	unsigned int iGroup;
	unsigned int iSet;
    printf("\ndrvFcom Version: "FCOM_DRV_VERSION"\n\n");

	for ( iGroup = 0; iGroup < N_DRV_FCOM_GROUPS_MAX; iGroup++ )
	{
		size_t			iBlob;
		grpBlob		*	pGrpBlob;
		drvGroup	*	pGroup = &drvFcomGroups[iGroup];
		if ( pGroup->IoScan == NULL )
			continue;

		pGrpBlob	= &pGroup->txBlobs[0];
		for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pGrpBlob++ )
		{
			if ( pGrpBlob->blob.fc_idnt == 0 )
				continue;
			printf( "Group %d, Blob Id "FCOM_ID_FMT", %s\n", iGroup, pGrpBlob->blob.fc_idnt, pGrpBlob->name );
		}
	}

	for ( iSet = 0; iSet < N_DRV_FCOM_SETS_MAX; iSet++ )
	{
		size_t			iBlob;
		size_t			iCb;
		unsigned int	nCb = 0;
		setBlob		*	pSetBlob;
		drvSet		*	pSet = &drvFcomSets[iSet];
		if ( pSet->IoScan == NULL )
			continue;

		for ( iCb = 0; iCb < N_DRV_FCOM_CB_MAX; iCb++ )
			if ( pSet->setCallbacks[iCb].cb != 0 )
				nCb++;
		printf( "Set %d, %u Blobs, %u Callbacks, timeout %f\n", iSet, pSet->nBlobsAdded, nCb, pSet->timeout );
		pSetBlob	= &pSet->rxBlobs[0];
		for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pSetBlob++ )
		{
			if ( pSetBlob->blobId == 0 )
				continue;
			printf( "Set %d, Blob Id "FCOM_ID_FMT", %s, %u good, %u timeouts\n", iSet, pSetBlob->blobId, pSetBlob->name, pSetBlob->nGoodBlobs, pSetBlob->nTimeouts );
		}
	}

    return 0;
}


/*
 * Register the driver entry table w/ EPICS
 */
static const struct drvet drvFcom =
{
	2,								/* 2 Table Entries */
    (DRVSUPFUN)	drvFcom_Report,		/* Driver Report Routine */
    (DRVSUPFUN)	drvFcom_Init		/* Driver Initialization Routine */
};

epicsExportAddress( drvet, drvFcom );
