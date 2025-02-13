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

  Name: fcomUtilRegister.cpp


  Abs:  Registered fcomUtilGetFcomID function to call from iocSh. returns FCOM ID
        given a full char * PV name - Note LCLS Naming convention for a pv name:
        Device Type : Location : Unit : Attribute 
        
        Refer to power point design in Sharepoint:
        LCLS | Electron Beam Systems | Controls | Fast Feedback | FCOM | FCOM ID Assignment Design

  Auth: 10 jan 2010, dfairley    created
  Rev:


-----------------------------------------------------------------------------*/
/*fcomUtilRegister.cpp */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include <iocsh.h>
#include <epicsExport.h>   /* for registering ffRestart, ffStop */

#include "fcomUtil.h"

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */


/* register fcomUtilGetFcomID for iocSh */
static const iocshArg fcomUtilGetFcomIDArg0 = { "pvNameString", iocshArgString};
static const iocshArg * const fcomUtilGetFcomIDArgs[1] = {&fcomUtilGetFcomIDArg0};
static const iocshFuncDef fcomUtilGetFcomIDFuncDef = 
  {"fcomUtilGetFcomID", 1, fcomUtilGetFcomIDArgs };
static void fcomUtilGetFcomIDCall(const iocshArgBuf *args) {
  fcomUtilGetFcomID(args[0].sval);
}
  
/* register fcomUtilGethostbyname for iocSh */
static const iocshArg fcomUtilGethostbynameArg0 = { "hostName", iocshArgString};
static const iocshArg fcomUtilGethostbynameArg1 = { "hostName", iocshArgInt   };
static const iocshArg * const fcomUtilGethostbynameArgs[2] = {
	&fcomUtilGethostbynameArg0,
	&fcomUtilGethostbynameArg1
};

/* register fcomUtilSetIPADDR1 for iocSh */
static const iocshArg fcomUtilSetIPADDR1Arg0 = { "postfix", iocshArgString};
static const iocshArg * const fcomUtilSetIPADDR1Args[1] = {&fcomUtilSetIPADDR1Arg0};
static const iocshFuncDef fcomUtilSetIPADDR1FuncDef = 
  {"fcomUtilSetIPADDR1", 1, fcomUtilSetIPADDR1Args };
static void fcomUtilSetIPADDR1Call(const iocshArgBuf *args) {
  fcomUtilSetIPADDR1(args[0].sval);
}
 
static const iocshFuncDef fcomUtilGethostbynameFuncDef = 
  {"fcomUtilGethostbyname", 2, fcomUtilGethostbynameArgs };
static void fcomUtilGethostbynameCall(const iocshArgBuf *args) {
char *val = fcomUtilGethostbyname(args[0].sval, (unsigned)args[1].ival);
	if ( val ) {
		printf("%s\n",val);
		free(val);
	}
}
 
/*=============================================================================

  Name: fcomUtilRegistrar

  Abs: add fcomUtilGetFcomID

  Args: none

  Rem: epics requires global functions to be registered for proper linking

  Side: 

  Ret:  none

==============================================================================*/
void fcomUtilRegistrar() {
	iocshRegister(&fcomUtilGetFcomIDFuncDef, fcomUtilGetFcomIDCall);
	iocshRegister(&fcomUtilGethostbynameFuncDef, fcomUtilGethostbynameCall);
	iocshRegister(&fcomUtilSetIPADDR1FuncDef, fcomUtilSetIPADDR1Call);
}

epicsExportRegistrar(fcomUtilRegistrar);

#ifdef __cplusplus
}
#endif	/* __cplusplus */
