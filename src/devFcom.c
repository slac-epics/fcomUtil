/*=============================================================================

  Name: devFcom.c

  Abs:  FCOM driver for fcom blob send/receive via db files

  Auth: Bruce Hill (bhill)

  Mod:  13-Jun-2016 - B.Hill	- Initial support for writing fcom blobs
  		15-Jun-2016 - B.Hill	- Adding support for reading fcom blobs 

-----------------------------------------------------------------------------*/

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <cantProceed.h>
#include <epicsAssert.h>
#include <epicsExport.h>
#include <epicsMath.h>
#include <dbScan.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <devSup.h>
#include <errlog.h>
#include <alarm.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#if 0
#include <epicsMutex.h>
#endif

#include "fcomUtil.h"
#include "devFcom.h"
#include "drvFcom.h"
#if 0
#include "debugPrint.h" 	/* For DP_DEBUG */
extern int	fcomUtilFlag;
#endif

int 	DEBUG_DEV_FCOM_RECV = 2;
int 	DEBUG_DEV_FCOM_SEND = 2;
epicsExportAddress(int, DEBUG_DEV_FCOM_RECV);
epicsExportAddress(int, DEBUG_DEV_FCOM_SEND);


typedef struct	devFcomPvt
{
	int				iGroup;
	int				iSet;
	int				iValue;
	FcomID			blobId;
	uint32_t		prior_tsHi;
	uint32_t		prior_tsLo;
	const char	*	pParamName;
	dbCommon	*	pRecord;
}	devFcomPvt;

/**********************************************************************
 * aiRecord device support functions
 **********************************************************************/
static long init_ai( struct aiRecord * pai)
{
	FcomID				blobId;
	devFcomPvt		*	pDevPvt;
	struct vmeio	*	pVmeIo;
	int					status;

    pai->dpvt = NULL;
    if ( pai->inp.type!=VME_IO )
    {
        recGblRecordError(S_db_badField, (void *)pai, "devFcom init_ai, Illegal INP");
        pai->pact=TRUE;
        return (S_db_badField);
    }

	pVmeIo		= &pai->inp.value.vmeio;
	blobId		= fcomLCLSPV2FcomID( pVmeIo->parm );

	if ( blobId == FCOM_ID_NONE )
	{
		errlogPrintf( "init_ai Error %s: Invalid Blob name %s!\n", pai->name, pVmeIo->parm );
		recGblRecordError(S_db_badField, (void *) pai, "devFcom init_ai, bad parameters");
		pai->pact = TRUE;
		return (S_db_badField);
	}

	pDevPvt	= (devFcomPvt *) callocMustSucceed( 1, sizeof(devFcomPvt), "devFcom init_ai" );
	pDevPvt->iValue		= pVmeIo->card;
	pDevPvt->iGroup		= -1;
	pDevPvt->iSet		= pVmeIo->signal;
	pDevPvt->blobId		= blobId;
	pDevPvt->pParamName	= pVmeIo->parm;
	pDevPvt->pRecord	= (dbCommon *) pai;
    pai->dpvt			= pDevPvt;

	if ( DEBUG_DEV_FCOM_RECV >= 1 )
		printf( "init_ai %s: Set %u, Blob "FCOM_ID_FMT"\n", pai->name, pDevPvt->iSet, pDevPvt->blobId );

	status = drvFcomAddBlobToSet( pDevPvt->iSet, pDevPvt->blobId, pDevPvt->pParamName );
	if ( status != 0 )
		errlogPrintf( "init_ai Error %s: Set %d, Blob "FCOM_ID_FMT", %s\n", pai->name, pDevPvt->iSet, pDevPvt->blobId, fcomStrerror(status) );

	return( 0 );
}

/** for sync scan records  **/
static long ai_ioint_info( int	cmd, aiRecord	*	pai, IOSCANPVT	*	iopvt )
{
	devFcomPvt	*	pDevPvt	= (devFcomPvt *) pai->dpvt;

	if ( DEBUG_DEV_FCOM_RECV >= 2 )
		printf( "ai_ioint_info %s: Set %u, Blob "FCOM_ID_FMT"\n", pai->name, pDevPvt->iSet, pDevPvt->blobId );

	if ( pDevPvt == NULL )
		return 0;

	*iopvt = drvFcomGetSetIoScan( pDevPvt->iSet );
	return 0;
}

static long read_ai(struct aiRecord *pai)
{
	long			status;
	int				fid		= 0x1FFFF;
	FcomBlob	*	pBlob	= NULL;
	devFcomPvt	*	pDevPvt	= (devFcomPvt *) pai->dpvt;
	if ( pDevPvt == NULL )
		return 0;

	status = fcomGetBlob( pDevPvt->blobId, &pBlob, 0 );
	if (	status != 0
		||	pBlob == NULL
		||	(	pDevPvt->prior_tsHi == pBlob->fc_tsHi
			&&	pDevPvt->prior_tsLo == pBlob->fc_tsLo	)
		||	pBlob->fc_nelm <= pDevPvt->iValue
		||	(	pBlob->fc_type != FCOM_EL_DOUBLE
			&&	pBlob->fc_type != FCOM_EL_FLOAT	) )
	{
		if ( pBlob )
			fcomReleaseBlob( &pBlob );
		recGblSetSevr( pai, CALC_ALARM, INVALID_ALARM );
		pai->udf	= TRUE;
		pai->val	= epicsNAN;
		return 2;
	}

	if ( pBlob->fc_type == FCOM_EL_DOUBLE )
		pai->val = pBlob->fc_dbl[ pDevPvt->iValue ];
	else
		pai->val = pBlob->fc_flt[ pDevPvt->iValue ];

	if ( pai->tse == epicsTimeEventDeviceTime )
	{
		/* do timestamp by device support */
		pai->time.secPastEpoch = pBlob->fc_tsHi;
		pai->time.nsec         = pBlob->fc_tsLo;
	}
	else
	{
		recGblGetTimeStamp( pai );
	}

	pDevPvt->prior_tsHi = pBlob->fc_tsHi;
	pDevPvt->prior_tsLo = pBlob->fc_tsLo;
	fcomReleaseBlob( &pBlob );
	fid			= pai->time.nsec & 0x1FFFF;
	pai->udf	= FALSE;

	if ( DEBUG_DEV_FCOM_RECV >= 3 )
		printf( "read_ai  %s: Set %u, Blob "FCOM_ID_FMT", Value %f, fid %d\n", pai->name, pDevPvt->iSet, pDevPvt->blobId, pai->val, fid );

	return 2;
}


/**********************************************************************
 * aoRecord device support functions
 **********************************************************************/
static long init_ao( struct aoRecord * pao)
{
	FcomID				blobId;
	devFcomPvt		*	pDevPvt;
	struct vmeio	*	pVmeIo;
	int					status;

    if (pao->out.type!=VME_IO)
    {
        recGblRecordError(S_db_badField, (void *)pao, "devFcom init_ao, Illegal OUT");
        pao->pact=TRUE;
        return (S_db_badField);
    }

	pVmeIo		= &pao->out.value.vmeio;
	blobId		= fcomLCLSPV2FcomID( pVmeIo->parm );

	if ( blobId == FCOM_ID_NONE )
	{
		errlogPrintf( "init_ao Error %s: Invalid Blob name %s!\n", pao->name, pVmeIo->parm );
		recGblRecordError(S_db_badField, (void *) pao, "devFcom init_ao, bad parameters");
		pao->pact = TRUE;
		return (S_db_badField);
	}

	pDevPvt	= (devFcomPvt *) callocMustSucceed( 1, sizeof(devFcomPvt), "devFcom init_ao" );
	pDevPvt->iValue		= pVmeIo->card;
	pDevPvt->iGroup		= pVmeIo->signal;
	pDevPvt->iSet		= -1;
	pDevPvt->blobId		= blobId;
	pDevPvt->prior_tsHi	= 0;
	pDevPvt->prior_tsLo	= 0;
	pDevPvt->pParamName	= pVmeIo->parm;
	pDevPvt->pRecord	= (dbCommon *) pao;
    pao->dpvt			= pDevPvt;

	if ( DEBUG_DEV_FCOM_SEND >= 1 )
		printf( "init_ao %s: Group %u, Blob "FCOM_ID_FMT"\n", pao->name, pDevPvt->iGroup, pDevPvt->blobId );

	status = drvFcomInitGroupBlob( pDevPvt->iGroup, pDevPvt->blobId, FCOM_EL_DOUBLE, pDevPvt->pParamName );
	if ( status != 0 )
		errlogPrintf( "init_ao Error %s: Group %d, Blob "FCOM_ID_FMT", %s\n", pao->name, pDevPvt->iGroup, pDevPvt->blobId, fcomStrerror(status) );

	return( 0 );
}

/** for sync scan records  **/
static long ao_ioint_info( int	cmd, aoRecord	*	pao, IOSCANPVT	*	iopvt )
{
	devFcomPvt	*	pDevPvt	= (devFcomPvt *) pao->dpvt;

	if ( DEBUG_DEV_FCOM_SEND >= 2 )
		printf( "ao_ioint_info %s: Group %u, Blob "FCOM_ID_FMT"\n", pao->name, pDevPvt->iGroup, pDevPvt->blobId );

	if ( pDevPvt == NULL )
		return 0;

	*iopvt = drvFcomGetGroupIoScan( pDevPvt->iGroup );
	return 0;
}

static long write_ao(struct aoRecord *pao)
{
	long			status	= 0;
	int				fid		= 0x1FFFF;
	devFcomPvt	*	pDevPvt	= (devFcomPvt *) pao->dpvt;
	if ( pDevPvt == NULL )
		return 0;

	recGblGetTimeStamp( pao );
	fid = pao->time.nsec & 0x1FFFF;

	if ( DEBUG_DEV_FCOM_SEND >= 2 && (fid % 3) != 0 ) 
		printf( "write_ao %s: Group %u, Blob "FCOM_ID_FMT", Value %f, fid %d mod 3 = %d!\n", pao->name, pDevPvt->iGroup, pDevPvt->blobId, pao->val, fid, fid % 3 );
	if ( DEBUG_DEV_FCOM_SEND >= 3 )
		printf( "write_ao %s: Group %u, Blob "FCOM_ID_FMT", Value %f, fid %d\n", pao->name, pDevPvt->iGroup, pDevPvt->blobId, pao->val, fid );

	status = drvFcomUpdateGroupBlobDouble(	pDevPvt->iGroup, pDevPvt->blobId,
											pDevPvt->iValue, pao->val, &pao->time );\
	if ( status != 0 )
	{
		errlogPrintf( "write_ao Error %s: Group %d, Blob "FCOM_ID_FMT", %s\n", pao->name, pDevPvt->iGroup, pDevPvt->blobId, fcomStrerror(status) );
		recGblSetSevr( pao, CALC_ALARM, INVALID_ALARM );
	}
	return 0;
}

/**********************************************************************
 * boRecord device support functions
 **********************************************************************/
static long init_bo( struct boRecord * pbo)
{
	FcomID				blobId	= FCOM_ID_NONE;
	devFcomPvt		*	pDevPvt;
	struct vmeio	*	pVmeIo;
	int					iGroup	= -1;
	int					iSet	= -1;

    if (pbo->out.type!=VME_IO)
    {
        recGblRecordError(S_db_badField, (void *)pbo, "devFcom init_bo, Illegal OUT");
        pbo->pact=TRUE;
        return (S_db_badField);
    }

	pVmeIo		= &pbo->out.value.vmeio;
	if ( strcmp( pVmeIo->parm, "SEND" ) == 0 )
	{
		/* All we need is a group number for a SEND boRecord */
		iGroup		= pVmeIo->signal;
if ( DEBUG_DEV_FCOM_RECV >= 1 ) printf( "init_bo %s: Group %d, Set %d, Blob "FCOM_ID_FMT", param :%s:\n", pbo->name, iGroup, iSet, blobId, pVmeIo->parm );
	}
	else if ( strcmp( pVmeIo->parm, "SYNC" ) == 0 )
	{
		/* All we need is a set number for a SYNC boRecord */
		iSet		= pVmeIo->signal;
		if ( pbo->evnt[0] != 0 ) {
			int code;
            if ( 1 == sscanf(pbo->evnt, "%i", &code) ) {
				if ( code != 0 ) {
					drvFcomSetSyncEventCode( code );
				}
            } else {
				cantProceed("init_bo (devBoFcom) -- handling named events (3.15) not implemented");
			}
		}
if ( DEBUG_DEV_FCOM_RECV >= 1 ) printf( "init_bo %s: Group %d, Set %d, Blob "FCOM_ID_FMT", param :%s:\n", pbo->name, iGroup, iSet, blobId, pVmeIo->parm );
	}
	else
	{
		blobId		= fcomLCLSPV2FcomID( pVmeIo->parm );
		if ( blobId == FCOM_ID_NONE )
		{
			errlogPrintf( "init_bo Error %s: Invalid Blob name %s!\n", pbo->name, pVmeIo->parm );
			recGblRecordError(S_db_badField, (void *) pbo, "devFcom init_bo, bad parameters");
			pbo->pact = TRUE;
			return (S_db_badField);
		}
		iGroup		= pVmeIo->signal;
if ( DEBUG_DEV_FCOM_RECV >= 1 ) printf( "init_bo %s: Group %d, Set %d, Blob "FCOM_ID_FMT", param :%s:\n", pbo->name, iGroup, iSet, blobId, pVmeIo->parm );
	}

	pDevPvt	= (devFcomPvt *) callocMustSucceed( 1, sizeof(devFcomPvt), "devFcom init_bo" );
	pDevPvt->iValue		= pVmeIo->card;
	pDevPvt->iGroup		= iGroup;
	pDevPvt->iSet		= iSet;
	pDevPvt->blobId		= blobId;
	pDevPvt->pParamName	= pVmeIo->parm;
	pDevPvt->pRecord	= (dbCommon *) pbo;
    pbo->dpvt			= pDevPvt;

	if ( pDevPvt->iGroup >= 0 && DEBUG_DEV_FCOM_SEND >= 1 )
		{
		if ( blobId != FCOM_ID_NONE )
			printf( "init_bo %s: Group %u, Blob "FCOM_ID_FMT", param %s\n", pbo->name,
					pDevPvt->iGroup, pDevPvt->blobId, pDevPvt->pParamName );
		else
			printf( "init_bo %s: Group %u, param %s\n", pbo->name,
					pDevPvt->iGroup, pDevPvt->pParamName );
		}
	if ( pDevPvt->iSet >= 0 && DEBUG_DEV_FCOM_RECV >= 1 )
		printf( "init_bo %s: Set   %u, param %s\n", pbo->name,
				pDevPvt->iSet, pDevPvt->pParamName );

	return( 0 );
}

static long write_bo(struct boRecord *pbo)
{
	long			status	= 0;
	devFcomPvt	*	pDevPvt	= (devFcomPvt *) pbo->dpvt;

	if ( pDevPvt == NULL )
		return 0;

	if ( pDevPvt->blobId != FCOM_ID_NONE )
	{
		if ( DEBUG_DEV_FCOM_SEND >= 4 )
			printf( "write_bo %s: Group %u, Write Blob "FCOM_ID_FMT"\n", pbo->name, pDevPvt->iGroup, pDevPvt->blobId );

		status = drvFcomWriteBlobToGroup(	pDevPvt->iGroup, pDevPvt->blobId );
	}
	else if ( strcmp( pDevPvt->pParamName, "SEND" ) == 0 )
	{
		if ( DEBUG_DEV_FCOM_SEND >= 4 )
			printf( "write_bo %s: Sending Group %u ...\n", pbo->name, pDevPvt->iGroup );

		status = drvFcomPutGroup(	pDevPvt->iGroup );
	}
	else if ( strcmp( pDevPvt->pParamName, "SYNC" ) == 0 )
	{
		if ( DEBUG_DEV_FCOM_RECV >= 4 )
			printf( "write_bo %s: Syncing Set %u ...\n", pbo->name, pDevPvt->iSet );

		drvFcomSignalSet( pDevPvt->iSet );
	}
	else
	{
		printf( "write_bo %s Error: Invalid paramName %s!\n", pbo->name, pDevPvt->pParamName );
	}

	if ( status != 0 )
		errlogPrintf(	"write_bo Error %s: %s %d, Blob "FCOM_ID_FMT", %s: %s\n",
						pbo->name,
						( pDevPvt->iGroup >= 0 ? "Group" : "Set" ),
						( pDevPvt->iGroup >= 0 ? pDevPvt->iGroup : pDevPvt->iSet ),
						pDevPvt->blobId, pDevPvt->pParamName,
						fcomStrerror(status) );

	return status;
}

/************************************************************
 * Register device support functions
 ************************************************************/
struct DEV_FCOM_AI_DSET
{
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_ai;
	DEVSUPFUN	ai_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
};
struct DEV_FCOM_AI_DSET devAiFcom	=
{
	6,
	NULL,
	NULL,
	init_ai,
	ai_ioint_info,
	read_ai,
	NULL
};
epicsExportAddress( dset, devAiFcom );

struct DEV_FCOM_AO_DSET
{
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_ao;
	DEVSUPFUN	ao_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
};
struct DEV_FCOM_AO_DSET devAoFcom	=
{
	6,
	NULL,
	NULL,
	init_ao,
	ao_ioint_info,
	write_ao,
	NULL
};
epicsExportAddress( dset, devAoFcom );

struct DEV_FCOM_BO_DSET
{
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_bo;
	DEVSUPFUN	bo_ioint_info;
	DEVSUPFUN	write_bo;
};
struct DEV_FCOM_BO_DSET devBoFcom	=
{
	5,
	NULL,
	NULL,
	init_bo,
	NULL,
	write_bo
};
epicsExportAddress( dset, devBoFcom );

#if 0
#include <subRecord.h>
#include <registryFunction.h>

long subInit(struct subRecord *psub)
{
  errlogPrintf("subInit was called\n");
  return 0;
}

long subProcess(struct subRecord *psub)
{
  psub->val = (int)(psub->time.nsec & 0x1FFFF);
  return 0;
}

epicsRegisterFunction(subInit);
epicsRegisterFunction(subProcess);
#endif
