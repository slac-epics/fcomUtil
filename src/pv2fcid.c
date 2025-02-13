//////////////////////////////////////////////////////////////////////////////
// This file is part of 'fcomUtil'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'fcomUtil', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
/* $Id: pv2fcid.c,v 1.1 2010/03/19 22:46:56 strauman Exp $ */
#include <stdio.h>
#include <fcomUtil.h>
#include <getopt.h>
#include <unistd.h>
#include <inttypes.h>
#include "debugPrint.h"

static void usage(char *nm)
{
	fprintf(stderr,"Usage: %s <pv_name>\n", nm);
}

static	int	debug	= DP_DEBUG;
extern	int	fcomUtilFlag;

int
main(int argc, char **argv)
{
int    ch;
int    rval;
FcomID idnt;

	if ( debug )
		fcomUtilFlag = debug;

	while ( -1 < (ch = getopt(argc, argv, "h")) ) {
		switch (ch) {
			default : rval = 1;
			case 'h':
				usage(argv[0]);
			return rval;
		}
	}

	if ( optind >= argc ) {
		usage(argv[0]);
		return 1;
	}


	idnt = fcomLCLSPV2FcomID (argv[optind]);

	if ( FCOM_ID_NONE == idnt ) {
		fprintf(stderr,"No ID found\n");
		return 1;
	}

	printf("0x%08"PRIx32"\n", idnt);
	printf( "MAJ: 0x%08"PRIx32"\n", FCOM_GET_MAJ(idnt) );
	printf( "GID: 0x%08"PRIx32"\n", FCOM_GET_GID(idnt) );
	printf( "SID: 0x%08"PRIx32"\n", FCOM_GET_SID(idnt) );

	return 0;
}
