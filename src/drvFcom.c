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

#define FCOM_DRV_VERSION "FCOM driver $Revision: 1.1 $/$Name:  $"

#define CA_PRIORITY     CA_PRIORITY_MAX         /* Highest CA priority */

#define TASK_PRIORITY   epicsThreadPriorityMax  /* Highest EPICS thread priority */

#define DEFAULT_CA_TIMEOUT	0.04		/* Default CA timeout, for 30Hz */
#define DEFAULT_EVR_TIMEOUT	0.2		/* Default EVR event timeout, for 30Hz */

#ifndef PULSEID
#define	PULSEID_INVALID		(0x1FFFF)
#define PULSEID(time)		((time).nsec & PULSEID_INVALID)
#endif	/* PULSEID */
#if 0
#ifndef FID_DIFF
/*
 *  A few fiducial helper definitions.
 *  FID_ROLL(a, b) is true if we have rolled over from fiducial a to fiducial b.  (That is, a
 *  is large, and b is small.)
 *  FID_GT(a, b) is true if fiducial a is greater than fiducial b, accounting for rollovers.
 *  FID_DIFF(a, b) is the difference between two fiducials, accounting for rollovers.
 */
#define FID_MAX        0x1ffe0
#define FID_ROLL_LO    0x00200
#define FID_ROLL_HI    (FID_MAX-FID_ROLL_LO)
#define FID_ROLL(a,b)  ((b) < FID_ROLL_LO && (a) > FID_ROLL_HI)
#define FID_GT(a,b)    (FID_ROLL(b, a) || ((a) > (b) && !FID_ROLL(a, b)))
#define FID_DIFF(a,b)  ((FID_ROLL(b, a) ? FID_MAX : 0) + (int)(a) - (int)(b) - (FID_ROLL(a, b) ? FID_MAX : 0))
#endif	/* FID_DIFF */
#endif

/* FcomID fcomBlobIDs[N_PULSE_BLOBS] = { 0 }; */

/* FcomBlobSetRef  fcomBlobSet = 0; */

typedef	struct	_drvBlob
{
	FcomBlob		blob;		/* FCom Blob to manage */
	size_t			nAlloc;		/* Number of values in currently allocated data block */
	const char	*	name;		/* Original blob name */
}	drvBlob;

typedef	struct	_drvGroup
{
	FcomGroup		groupHandle;
	int				nBlobsAdded;
	IOSCANPVT		IoScan;
	drvBlob			txBlobs[N_DRV_FCOM_BLOBS_MAX];
}	drvGroup;

epicsMutexId		drvFcomMutex;
static	drvGroup	drvFcomGroups[N_DRV_FCOM_GROUPS_MAX];

int 	DEBUG_DRV_FCOM_RECV = 2;
int 	DEBUG_DRV_FCOM_SEND = 2;
epicsExportAddress(int, DEBUG_DRV_FCOM_RECV);
epicsExportAddress(int, DEBUG_DRV_FCOM_SEND);

/* Share with device support */

/*==========================================================*/

static epicsEventId BlobSetSyncEvent = NULL;

epicsTimeStamp  fcomFiducialTime;
t_HiResTime		fcomFiducialTsc = 0LL;	/* 64 bit hi-res cpu timestamp counter */

/*
 * To allow full diagnostic timing, call drvFcomSetFuncGetBeamStart
 * w/ a function ptr to a routine that returns the cpu tsc for
 * the start of the beam interval
 */
GET_BEAM_TSC	GetBeamStartTsc	= NULL;	/* 64 bit hi-res cpu timestamp counter */
void	drvFcomSetFuncGetBeamStart( GET_BEAM_TSC pGetBeamStartFunc )
{
	GetBeamStartTsc = pGetBeamStartFunc;
}

/*
 * To allow full diagnostic timing, call drvFcomSetFuncTicksToSec
 * w/ a function ptr to a routine that returns the cpu tsc for
 * the start of the beam interval
 */
TSC_TO_TICKS	TicksToSec	= NULL;	/* 64 bit hi-res cpu timestamp counter */
void	drvFcomSetFuncTicksToSec( TSC_TO_TICKS pTicksToSecFunc )
{
	TicksToSec = pTicksToSecFunc;
}


void drvFcomSignalSet( unsigned int	iSet )
{
	/* Signal the BlobSetSyncEvent to trigger the fcomGetBlobSet call */
	epicsEventSignal( BlobSetSyncEvent );
	return;
}

/* Internal use only.  Call w/ mutex locked */
drvBlob	*	drvFcomFindAvailBlobForId( drvBlob * pFirstTxBlob, FcomID id  )
{
	size_t		iBlob;
	drvBlob	*	pBlob	= pFirstTxBlob;
	for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pBlob++ )
	{
		if( pBlob->blob.fc_idnt == id )
			return pBlob;
		if( pBlob->blob.fc_idnt == 0 )
			return pBlob;
	}
	return NULL;
}

/* Internal use only.  Call w/ mutex locked */
drvBlob	*	drvFcomFindBlobForId( drvBlob * pFirstTxBlob, FcomID id  )
{
	size_t		iBlob;
	drvBlob	*	pBlob	= pFirstTxBlob;
	for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pBlob++ )
	{
		if( pBlob->blob.fc_idnt == id )
			return pBlob;
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
	drvBlob	*	pBlob	= drvFcomFindAvailBlobForId( &pGroup->txBlobs[0], id );
	if ( pBlob == NULL )
	{
		errlogPrintf( "drvFcomInitGroupBlob: Group %d, Blob "FCOM_ID_FMT": No more blobs available\n", iGroup, id );
		epicsMutexUnlock( drvFcomMutex );
		return -1;
	}

	if ( pBlob->blob.fc_idnt == 0 )
	{
		/* Initialize drvFcom blob for up to N values */
		pBlob->nAlloc		= 8;	/* Start w/ 8, can be increased as needed */
		pBlob->name			= name;
		pBlob->blob.fc_raw	= calloc( pBlob->nAlloc, FCOM_EL_SIZE(fComType) );
		pBlob->blob.fc_vers	= FCOM_PROTO_VERSION;
		pBlob->blob.fc_idnt	= id;
		pBlob->blob.fc_type	= fComType;
		pBlob->blob.fc_stat	= 0;
		pBlob->blob.fc_tsLo	= 0;
		pBlob->blob.fc_tsHi	= 0;
		pBlob->blob.fc_nelm	= 0;

		if ( DEBUG_DRV_FCOM_SEND >= 1 )
			printf( "drvFcomInitGroupBlob: Group %u, Blob "FCOM_ID_FMT", %s\n", iGroup, id, name );
	}
	else
	{
		if ( DEBUG_DRV_FCOM_SEND >= 2 )
			printf( "drvFcomInitGroupBlob: Group %u, Blob "FCOM_ID_FMT", %s\n", iGroup, id, name );
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
	drvBlob	*	pBlob	= drvFcomFindBlobForId( &pGroup->txBlobs[0], id );
	if ( pBlob == NULL )
	{
		errlogPrintf( "drvFcomUpdateGroupBlobDouble: Group %d, Blob "FCOM_ID_FMT" not found!\n", iGroup, id );
		epicsMutexUnlock( drvFcomMutex );
		return FCOM_ERR_ID_NOT_FOUND;
	}

	if( pBlob->nAlloc <= iValue )
	{
		pBlob->nAlloc *= 2;	/* Double the capacity */
		if ( DEBUG_DRV_FCOM_SEND >= 2 )
			printf( "drvFcomUpdateGroupBlobDouble: Group %u, Blob "FCOM_ID_FMT", bumping capacity to %zu\n", iGroup, id, pBlob->nAlloc );
		if( pBlob->nAlloc <= iValue )
		{
			if ( iValue > 1000 )
			{	/* Probably an error */
				errlogPrintf( "drvFcomUpdateGroupBlobDouble: Group %d, Blob "FCOM_ID_FMT", Value index %d too large!\n", iGroup, id, iValue );
				epicsMutexUnlock( drvFcomMutex );
				return -1;
			}
			pBlob->nAlloc = iValue;
		}
		pBlob->blob.fc_raw	= realloc( pBlob->blob.fc_raw, pBlob->nAlloc * FCOM_EL_SIZE(pBlob->blob.fc_type) );
		if ( pBlob->blob.fc_raw == NULL )
		{
			pBlob->nAlloc = 0;
			errlogPrintf( "drvFcomUpdateGroupBlobDouble: Group %d, Blob "FCOM_ID_FMT", Unable to realloc for Value index %d!\n", iGroup, id, iValue );
			epicsMutexUnlock( drvFcomMutex );
			return -1;
		}
	}

	if( pBlob->blob.fc_nelm < (iValue + 1) )
		pBlob->blob.fc_nelm = (iValue + 1);
	pBlob->blob.fc_flt[iValue] = value;
	if ( pTimeStamp != NULL )
	{
		pBlob->blob.fc_tsHi = pTimeStamp->secPastEpoch;
		pBlob->blob.fc_tsLo = pTimeStamp->nsec;
	}

	epicsMutexUnlock( drvFcomMutex );

	if ( DEBUG_DRV_FCOM_SEND >= 4 )
		printf( "drvFcomUpdateGroupBlobDouble: Group %u, Blob "FCOM_ID_FMT", %s, fc_flt[%d]=%f, fid %d\n",
				iGroup, id, pBlob->name, iValue, pBlob->blob.fc_flt[iValue], fid );

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
	drvBlob	*	pBlob	= drvFcomFindBlobForId( &pGroup->txBlobs[0], id );
	if ( pBlob == NULL )
	{
		errlogPrintf( "drvFcomUpdateGroupBlobLong: Group %d, Blob "FCOM_ID_FMT" not found!\n", iGroup, id );
		epicsMutexUnlock( drvFcomMutex );
		return FCOM_ERR_ID_NOT_FOUND;
	}

	if( pBlob->nAlloc <= iValue )
	{
		pBlob->nAlloc *= 2;	/* Double the capacity */
		if( pBlob->nAlloc <= iValue )
		{
			if ( iValue > 1000 )
			{	/* Probably an error */
				errlogPrintf( "drvFcomUpdateGroupBlobLong: Group %d, Blob "FCOM_ID_FMT", Value index %d too large!\n", iGroup, id, iValue );
				epicsMutexUnlock( drvFcomMutex );
				return FCOM_ERR_INVALID_ARG;
			}
			pBlob->nAlloc = iValue;
		}
		pBlob->blob.fc_raw	= realloc( pBlob->blob.fc_raw, pBlob->nAlloc * FCOM_EL_SIZE(pBlob->blob.fc_type) );
		if ( pBlob->blob.fc_raw == NULL )
		{
			pBlob->nAlloc = 0;
			errlogPrintf( "drvFcomUpdateGroupBlobLong: Group %d, Blob "FCOM_ID_FMT", Unable to realloc for Value index %d!\n", iGroup, id, iValue );
			epicsMutexUnlock( drvFcomMutex );
			return FCOM_ERR_NO_MEMORY;
		}
	}

	if( pBlob->blob.fc_nelm < iValue )
		pBlob->blob.fc_nelm = iValue;
	pBlob->blob.fc_u32[iValue] = value;
	if ( pTimeStamp != NULL )
	{
		pBlob->blob.fc_tsHi = pTimeStamp->secPastEpoch;
		pBlob->blob.fc_tsLo = pTimeStamp->nsec;
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
	drvBlob	*	pBlob	= drvFcomFindBlobForId( &pGroup->txBlobs[0], id );
	if ( pBlob == NULL )
	{
		errlogPrintf( "drvFcomWriteBlobToGroup: Group %d, Blob "FCOM_ID_FMT" not found!\n", iGroup, id );
		epicsMutexUnlock( drvFcomMutex );
		return FCOM_ERR_ID_NOT_FOUND;
	}
	if ( pBlob->blob.fc_raw == NULL )
	{
		errlogPrintf( "drvFcomWriteBlobToGroup: Group %d, Blob "FCOM_ID_FMT", data allocation error!\n", iGroup, id );
		epicsMutexUnlock( drvFcomMutex );
		return -1;
	}

	/* Add the blob to the group */
	status = fcomAddGroup( pGroup->groupHandle, &pBlob->blob );
	if ( status != 0 )
	{
		errlogPrintf( "drvFcomWriteBlobToGroup Error: Group %d, Blob "FCOM_ID_FMT", %s\n", iGroup, id, fcomStrerror(status) );
		epicsMutexUnlock( drvFcomMutex );
		return status;
	}
	pGroup->nBlobsAdded++;

	epicsMutexUnlock( drvFcomMutex );

	if ( DEBUG_DRV_FCOM_SEND >= 4 )
		printf( "drvFcomWriteBlobToGroup: Group %u, Blob "FCOM_ID_FMT", %s\n", iGroup, id, pBlob->name );

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
	unsigned int iGroup = 0;
    printf( "drvFcom_Init:\n" );

	drvFcomMutex = epicsMutexMustCreate();

	epicsMutexLock( drvFcomMutex );
	/* Initialize the driver tables */
	for ( iGroup = 0; iGroup < N_DRV_FCOM_GROUPS_MAX; iGroup++ )
	{
		size_t			iBlob;
		drvBlob		*	pBlob;
		drvFcomGroups[iGroup].groupHandle	= NULL;
		drvFcomGroups[iGroup].IoScan		= NULL;
		pBlob	= &(drvFcomGroups[iGroup].txBlobs[0]);
		for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pBlob++ )
		{
			pBlob->blob.fc_idnt	= 0;
			pBlob->blob.fc_raw	= 0;
			pBlob->name			= "";
		}
	}
	epicsMutexUnlock( drvFcomMutex );

    epicsAtExit(drvFcomShutdown, NULL);

    return 0;
}

static long drvFcom_Report(int level)
{
	unsigned int iGroup = 0;
    printf("\ndrvFcom Version: "FCOM_DRV_VERSION"\n\n");

	for ( iGroup = 0; iGroup < N_DRV_FCOM_GROUPS_MAX; iGroup++ )
	{
		size_t			iBlob;
		drvBlob		*	pBlob;
		drvGroup	*	pGroup = &drvFcomGroups[iGroup];
		if ( pGroup->IoScan == NULL )
			continue;

		pBlob	= &pGroup->txBlobs[0];
		for ( iBlob = 0; iBlob < N_DRV_FCOM_BLOBS_MAX; iBlob++, pBlob++ )
		{
			if ( pBlob->blob.fc_idnt == 0 )
				continue;
			printf( "Group %d, Blob Id "FCOM_ID_FMT", %s\n", iGroup, pBlob->blob.fc_idnt, pBlob->name );
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

