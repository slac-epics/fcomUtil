/*=============================================================================

  Name: devFcomSubRecord.c

  Abs:  FCOM driver subroutine record functions
        Can be used as a template for custom FCOM Blob callbacks

  Auth: Bruce Hill (bhill)

  Mod:  16-Jun-2016 - B.Hill	- Initial support for subroutine record

-----------------------------------------------------------------------------*/

#include <string.h>
#include <stdlib.h>

#include <aSubRecord.h>
#include <cantProceed.h>
#include <dbScan.h>
#include <dbAccess.h>
#include <dbAccessDefs.h>
#include <epicsExport.h>
#include <errlog.h>
#include <recGbl.h>
#include <registryFunction.h>

#if 0
#include <math.h>

#include <epicsAssert.h>
#include <devSup.h>
#include <alarm.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <epicsMutex.h>
#endif

#include "fcomUtil.h"
#include "devFcom.h"
#include "drvFcom.h"

#if 0
#include "debugPrint.h" 	/* For DP_DEBUG */
extern int	fcomUtilFlag;
#endif

int 	DEBUG_DEV_FCOM_SUB = 2;
epicsExportAddress( int, DEBUG_DEV_FCOM_SUB );

typedef struct	devFcomSubPvt
{
	int				iSet;
	FcomID			blobId;
	char			blobName[MAX_STRING_SIZE];
}	devFcomSubPvt;


static void fcomDevSubCallback( void * pArg )
{
	int					status;
	aSubRecord		*	pSub 	= (aSubRecord *) pArg;
	dbCommon		*	pRec	= (dbCommon *) pSub;

	if ( pSub == NULL )
		return;

	dbScanLock(	pRec );
	status = dbProcess( pRec );
	dbScanUnlock( pRec );

	if ( status != 0 )
	{
		errlogPrintf( "fcomDevSubCallback %s: Error %d!\n", pSub->name, status );
	}
	return;
}

static void fcomDevSubLookupBlobId( aSubRecord * pSub )
{
	char			*	pSpace;
	devFcomSubPvt	*	pDevPvt	= pSub->dpvt;
	char				blobName[MAX_STRING_SIZE];

	if ( pSub->b == NULL )
		return;

#if 1
	/* The blobName is in the text for INPB.  Just grab it */
	memset( blobName, 0, MAX_STRING_SIZE );
	strncpy( blobName, (const char *) pSub->b, MAX_STRING_SIZE );
	pSpace = memchr( blobName, ' ', MAX_STRING_SIZE );
	if ( pSpace )
		*pSpace = 0;
#else
	DBENTRY	*	pDbEntry = ?
	dbGetString( pDbEntry );
#endif

	/* TODO: Use above pattern to get function names from INPC and INPD for
	 * evrGetFiducialTsc and HiResTicksToSeconds.
	 * use registryFunctionFind() to find their addresses and provide
	 * wrappers to call them for diagnostic timing support without
	 * adding dependencies on EVENT or DIAG_TIMER modules.
	 * Need function(HiResTicksToSeconds) added to DIAG_TIMER module
	 * and something similar to EVENT and EVRCLIENT to get tsc for fiducial.
	 */
	pDevPvt->blobId	= fcomLCLSPV2FcomID( blobName );
	if ( pDevPvt->blobId != FCOM_ID_NONE ) {
		strncpy( pDevPvt->blobName, blobName, MAX_STRING_SIZE );
	} else {
		errlogPrintf( "fcomDevSubLookupBlobId Error %s: Invalid Blob name %s!\n", pSub->name, blobName );
	}
	return;
}

static long fcomDevSubInit( aSubRecord * pSub )
{
	devFcomSubPvt	*	pDevPvt;
	int					status;
	unsigned int		iSet;

	/* Check field types */
	if (	pSub->fta  != DBR_ULONG
		||	pSub->ftb  != DBR_STRING
		||	pSub->ftva != DBR_ULONG
		||	pSub->ftvb != DBR_ULONG
		||	pSub->ftvc != DBR_ULONG
		||	pSub->ftvd != DBR_ULONG
		||	pSub->ftve != DBR_ULONG )
	{
		recGblRecordError(S_db_badField, (void *) pSub, "devFcom fcomDevSubInit, bad field types");
		pSub->pact = TRUE;
		return (S_db_badField);
	}

	/* Get the parameters */
	iSet	= *((epicsUInt32 *) pSub->a);

	if ( DEBUG_DEV_FCOM_SUB >= 1 )
		printf( "fcomDevSubInit %s: Set %u, Blob %s.\n", pSub->name, iSet, pSub->inpb.text );

	/* Validate the parameters */
	if ( iSet > N_DRV_FCOM_SETS_MAX )
	{
		errlogPrintf( "fcomDevSubInit Error %s: Invalid Set %d!\n", pSub->name, iSet );
		recGblRecordError(S_db_badField, (void *) pSub, "devFcom fcomDevSubInit, invalid set number");
		pSub->pact = TRUE;
		return (S_db_badField);
	}

	/* Allocate private data block */
    pSub->dpvt = NULL;
	pDevPvt	= (devFcomSubPvt *) callocMustSucceed( 1, sizeof(devFcomSubPvt), "devFcom init_ao" );
	pDevPvt->iSet		= iSet;
	pDevPvt->blobId		= FCOM_ID_NONE;
	memset( pDevPvt->blobName, 0, MAX_STRING_SIZE );
    pSub->dpvt			= pDevPvt;

	status = drvFcomAddSetCallback( pDevPvt->iSet, fcomDevSubCallback, pSub );
	if ( status != 0 )
		errlogPrintf( "fcomDevSubInit Error %s: Set %d, Unable to add callback!\n", pSub->name, pDevPvt->iSet );

	return( 0 );
}

static long fcomDevSubProc( aSubRecord * pSub )
{
	devFcomSubPvt	*	pDevPvt	= pSub->dpvt;

	if ( pDevPvt == NULL )
		return -1;
	
	if ( pDevPvt->blobId == FCOM_ID_NONE ) {
		fcomDevSubLookupBlobId( pSub );
		if ( pDevPvt->blobId == FCOM_ID_NONE ) {
			if ( DEBUG_DEV_FCOM_SUB >= 2 )
				printf( "fcomDevSubProc %s: Set %u, BlobId not found!\n", pSub->name, pDevPvt->iSet );
			return( 0 );
		}
	}

	if ( pSub->tse == epicsTimeEventDeviceTime )
	{
		/* do timestamp by device support */
		FcomBlob	*	pBlob	= NULL;
		fcomGetBlob( pDevPvt->blobId, &pBlob, 0 );
		if ( pBlob )
		{
			pSub->time.secPastEpoch = pBlob->fc_tsHi;
			pSub->time.nsec         = pBlob->fc_tsLo;
		}
	}

	if ( DEBUG_DEV_FCOM_SUB >= 3 )
		printf( "fcomDevSubInit %s: Set %u, Blob "FCOM_ID_FMT"\n", pSub->name, pDevPvt->iSet, pDevPvt->blobId );

	return( 0 );
}

epicsRegisterFunction( fcomDevSubInit );
epicsRegisterFunction( fcomDevSubProc );
