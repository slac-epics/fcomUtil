/* $Id$ */
#include <stdio.h>
#include <fcomUtil.h>
#include <getopt.h>
#include <unistd.h>
#include <inttypes.h>

static void usage(char *nm)
{
	fprintf(stderr,"Usage: %s <pv_name>\n", nm);
}

int
main(int argc, char **argv)
{
int    ch;
int    rval;
FcomID idnt;

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

	return 0;
}
