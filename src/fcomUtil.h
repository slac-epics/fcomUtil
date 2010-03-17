/*=============================================================================

  Name: fcomUtil.h

  Abs:  function prototypes, defines for fnetUtil.c
  -- A unique Blob ID is formulated based upon the unique string representation of a PV
  -- Each unique Blob ID is composed of a group ID (GID) and a signal ID (SID)
  -- GIDs map to a unique multicast ID, so are unique
     Refer to Sharepoint | LCLS | Electron Beam Systems | Controls | Fast Feedback | 
	 FCOM | FCOM ID Assignment Design

  Auth: 10 DEC-2009, drogind created
  Rev:

-----------------------------------------------------------------------------*/
#include "copyright_SLAC.h"
/*-----------------------------------------------------------------------------
  Mod:  (newest to oldest)
        DD-MMM-YYYY, My Name:
           Changed such and such to so and so. etc. etc.
        DD-MMM-YYYY, Your Name:
           More changes ... The ordering of the revision history
           should be such that the NEWEST changes are at the HEAD of
           the list.

=============================================================================*/

#ifndef INCfnetUtilH
#define INCfnetUtilH

#ifdef __cplusplus
extern "C" {
#endif 	/* __cplusplus */

#include    <fcom_api.h>         /* Note: #define FCOM_ID_NONE   0  to be added by Till */

#define FCOM_ID_NONE    0       /* until it is defined by Till ... */
#define PVNAME_MAX   64

/*******************************************************************************************/
/*
 * GID (11bits) may be represented as
 *  Setpoint/Readback (1 bit) | Device Type (4 bits) | Area (6 bits)
 *
 *******************************************************************************************/

 /*  Setpoint/Readback (1 bit):
 */
#define FCOM_MASK_SETPOINT  ((unsigned long)0x00000001)
#define FCOM_MASK_READBACK  ((unsigned long)0x00000000)

/*  Device Type (4 bits) - provides growth for up to 16 device types)
 *  BPM, BLEN, TCAV, ACCL, XCOR, YCOR, FBCK
 */
#define FCOM_MASK_DEVICE_START           ((unsigned long)0x00000001)
#define FCOM_MASK_DEVICE_BPM             ((unsigned long)0x00000001)
#define FCOM_MASK_DEVICE_BLEN            ((unsigned long)0x00000002)
#define FCOM_MASK_DEVICE_TCAV            ((unsigned long)0x00000003)
#define FCOM_MASK_DEVICE_ACCL            ((unsigned long)0x00000004)
#define FCOM_MASK_DEVICE_XCOR            ((unsigned long)0x00000005)
#define FCOM_MASK_DEVICE_YCOR            ((unsigned long)0x00000006)
#define FCOM_MASK_DEVICE_FBCK            ((unsigned long)0x00000007)

/*   Area (6 bits) - growth for up to 64 areas - 8 (to account for FCOM_GID_MIN); 
     48 are initially used for machine areas ranging
 *   from IN20 to LTU -> 4 beamlines ; plus 4 Controllers, for total = 40 (+ 8)
 */
/*#define FCOM_MASK_AREA_START             ((unsigned long)0x00000001) */
#define FCOM_MASK_AREA_START             FCOM_GID_MIN 
#define FCOM_MASK_AREA_IN20              ((unsigned long)0x00000001)
#define FCOM_MASK_AREA_LI21              ((unsigned long)0x00000002)
#define FCOM_MASK_AREA_LI22              ((unsigned long)0x00000003)
#define FCOM_MASK_AREA_LI23              ((unsigned long)0x00000004)
#define FCOM_MASK_AREA_LI24              ((unsigned long)0x00000005)
#define FCOM_MASK_AREA_LI25              ((unsigned long)0x00000006)
#define FCOM_MASK_AREA_LI26              ((unsigned long)0x00000007)
#define FCOM_MASK_AREA_LI27              ((unsigned long)0x00000008)
#define FCOM_MASK_AREA_LI28              ((unsigned long)0x00000009)
#define FCOM_MASK_AREA_LI29              ((unsigned long)0x0000000A)
#define FCOM_MASK_AREA_LI30              ((unsigned long)0x0000000B)
#define FCOM_MASK_AREA_BSY0              ((unsigned long)0x0000000C)
#define FCOM_MASK_AREA_LTU0              ((unsigned long)0x0000000D)
#define FCOM_MASK_AREA_LTU1              ((unsigned long)0x0000000E)
#define FCOM_MASK_AREA_UND1              ((unsigned long)0x0000000F)
#define FCOM_MASK_AREA_DMP1              ((unsigned long)0x00000010)
#define FCOM_MASK_AREA_FEE1              ((unsigned long)0x00000011)
#define FCOM_MASK_AREA_NEH1              ((unsigned long)0x00000012)
#define FCOM_MASK_AREA_FEH1              ((unsigned long)0x00000013)
#define FCOM_MASK_AREA_FB01              ((unsigned long)0x00000014)
#define FCOM_MASK_AREA_FB02              ((unsigned long)0x00000015)
#define FCOM_MASK_AREA_FB03              ((unsigned long)0x00000016)
#define FCOM_MASK_AREA_FB04              ((unsigned long)0x00000017)

/* Detectors SID is
unit # - up 9999 decimal = 0x270F hex = 14 bits       */
/*******************************************************************************************/
/*
 * SID (16bits) has different derivations based on device type. It then uses pv name
 * and PAU slot# (if any) to comprise the SID, except for RF PVs
 * Note  FCOM SID MIN 8; FCOM SID MAX 0xFFFF
 *
 *******************************************************************************************/

/* For Controller loops */
#define FCOM_MASK_AREA_CTRL_MIN        ((unsigned long)0x00000000)

#define FCOM_MASK_LOOPTYPE_TR          ((unsigned long)0x00000000)
#define FCOM_MASK_LOOPTYPE_GN          ((unsigned long)0x00000001)
#define FCOM_MASK_LOOPTYPE_LN          ((unsigned long)0x00000002)

/* For RF PVs */

#define FCOM_MASK_SIG_START                        ((unsigned long)0x00000001)
#define FCOM_MASK_SIG_ACCL_IN20_400_L0B_A          ((unsigned long)0x00000001)       /*  *ACCL:IN20:400:A  */
/* TCav0: */

/* *L1S:   */
#define FCOM_MASK_SIG_ACCL_LI21_1_LIS_P            ((unsigned long)0x00000002)       /* * ACCL:LI21:1:P  */
#define FCOM_MASK_SIG_ACCL_LI21_1_LIS_A            ((unsigned long)0x00000003)       /** ACCL:LI21:1:A  */
/* L1X:    */

/* *L2: (new) */
#define FCOM_MASK_SIG_ACCL_LI24_1_P                ((unsigned long)0x00000004)       /* *ACCL:LI24:1:P  */
#define FCOM_MASK_SIG_ACCL_LI24_1_A                ((unsigned long)0x00000005)       /*  *ACCL:LI24:1:A  */
/* TCav3:     */

/* *L3: (new)  */
#define FCOM_MASK_SIG_ACCL_LI30_1_A                ((unsigned long)0x00000006)      /* *ACCL:LI30:1:A   */

/* Public prototypes for fcomUtil.c ************************************************/

FcomID fcomLCLSPV2FcomID (const char* pvName_p );
void  fcomUtilGetFcomID(const char * pvNameString);
int fcomParseLCLSPvName (const char * pvName_ca, 
						 char * deviceType_ca, 
						 char * area_ca, 
						 char * unit_ca, 
						 char * attrib_ca, 
						 char * slot_ca );

/* Try to translate a host name into a 'dot notation' IPv4 address.
 * The returned string is 'malloc()ed' and may be free()ed when no
 * longer in use.
 * If a nonzero port number is passed then the string ":<port>" is
 * appended to the returned address.
 *
 * RETURNS: string or NULL if host name could not be looked up.
 */
char *fcomUtilGethostbyname(const char *name, unsigned port);

/* Lookup the result of 'gethostname' with 'postfix' appended
 * and set the environment variable IPADDR1 to the result
 * of the lookup (in 'dot notation').
 * The IPADDR1 variable is unset if the lookup is not successful.
 */
void  fcomUtilSetIPADDR1(const char *postfix);

#ifdef __cplusplus
}
#endif 	/* __cplusplus */

#endif  /* INCfnetUtilH */
