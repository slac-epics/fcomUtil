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
}

epicsExportRegistrar(fcomUtilRegistrar);

#ifdef __cplusplus
}
#endif	/* __cplusplus */
