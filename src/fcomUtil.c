/*=============================================================================

  Name: fcomUtil.c


  Abs:  FCOM (Fast Feedback Network) Utilities for returning FCOM GID, SID and FCOMIDs
        given a full char * PV name - Note LCLS Naming convention for a pv name:
        Device Type : Location : Unit : Attribute 
        
        Refer to power point design in Sharepoint:
        LCLS | Electron Beam Systems | Controls | Fast Feedback | FCOM | FCOM ID Assignment Design

  Auth: 13 Nov 2009, dROGIND    created
  Rev:  17 Feb 2010, drogind    exposed fcomParseLCLSPvName() as public


-----------------------------------------------------------------------------*/

#include "copyright_SLAC.h"	/* SLAC copyright comments */

/*-----------------------------------------------------------------------------

  Mod:
  (newest to oldest)
        26 june 2009, DROGIND    created
=============================================================================*/

#include "debugPrint.h"
#ifdef  DEBUG_PRINT
#include <stdio.h>
  int fcomUtilFlag = DP_INFO;
#endif

/* c includes */
#include <string.h>         /* memcpy */
#include <stdlib.h>         /* atoi */
/* modules */
#include    <fcom_api.h>   /* Note: #define FCOM_ID_NONE   0  to be added by Till */
#include    <fcomUtil.h>

static char * fcomStrtok_r(char *s1, const char *s2, char **lasts);

/* registryFunctionRef ffRef[] = {
  {"fcomUtilGetFcomID", (REGISTRYFUNCTION)fcomUtilGetFcomID},
};
*/

  
/***********************************************************************************************/
/*                                                                                             */
/* Used for parsing PV Names - NOTE Dependencies -                                             */
/*                                                                                             */
/************************************************************************************************/
#define MAX_SETPOINTS   6   /* ensure count is compatible with setpoint_ca list count below !!! */
#define MAX_AREAS      23   /* ensure count is compatible with area_ca list count below !!!     */
#define MAX_DEVTYPES    7   /* ensure count is compatible with devtype_ca list count below !!!  */
#define MAX_RFNAMES     6   /* ensure count is compatible with rfname_ca list count below !!!   */
#define MAX_DETECTOR_NAMES  2/* ensure count is compatible with rfname_ca list count below !!!   */
#define MAX_LOOP_TYPES  3   /* ensure count is compatible with rfname_ca list count below !!!   */
#define MAX_PAU_SLOTS   4
  

/* list of setpoints - add future here [row] [col]; also update MAX_SETPOINTS above */
static const char * setpoint_ca[MAX_SETPOINTS] = { "BCTRL", "L0B_A", "L1S_P", "L1S_A", "P", "A"};

/* list of areas  - add future here ; also update MAX_AREAS above      */
static const char * area_ca[MAX_AREAS] = { "IN20", "LI21", "LI22", "LI23", "LI24", "LI25", "LI26", "LI27", "LI28",
		"LI29", "LI30", "BSY1", "LTU0", "LTU1", "UND1", "DMP1", "FEE1", "NEH1", "FEH1", "FB01", "FB02", "FB03", "FB04"};

/* list of device types- add future here; also update MAX_DEVTYPES above */
static const char * devtype_ca[MAX_DEVTYPES] = { "BPMS", "BLEN", "TCAV", "ACCL", "XCOR", "YCOR", "FBCK"};

/* list of RF names */
static const char * rfname_ca[MAX_RFNAMES]= { "ACCL:IN20:400:L0B_A", "ACCL:LI21:1:L1S_P", "ACCL:LI21:1:L1S_A",
		"ACCL:LI24:1:P", "ACCL:LI24:1:A", "ACCL:LI30:1:A"};

/* list of feedback loop types */
static const char * looptype_ca[MAX_LOOP_TYPES] = {"TR", "LG", "GN" };

/* list of detector names */
static const char * detector_ca[MAX_DETECTOR_NAMES]= {"BPMS", "BLEN" };

/****************************************************************************************************************/
static char * fcomStrtok_r(char *s1, const char *s2, char **lasts)  {
	char *ret;
	 DEBUGPRINT (DP_DEBUG, fcomUtilFlag,("fcomStrtok_r delim = %s\n", s2));
	if (s1 == NULL) {
		s1 = *lasts;
		 DEBUGPRINT (DP_DEBUG, fcomUtilFlag,("s1 = *lasts = %s \n", s1));
	}
	while(*s1 && strchr(s2, *s1)) {
		++s1;
		 DEBUGPRINT (DP_DEBUG, fcomUtilFlag,("++s1\n"));
	}
	if(*s1 == '\0') {
		 DEBUGPRINT (DP_DEBUG, fcomUtilFlag,("return NULL\n"));
		 return NULL;
	}
		
	ret = s1;
	DEBUGPRINT (DP_DEBUG, fcomUtilFlag,("ret = %p\n", ret));
	while(*s1 && !strchr(s2, *s1)) {
		++s1;
		 DEBUGPRINT (DP_DEBUG, fcomUtilFlag,("2nd ++s1\n"));
	}
	if(*s1) {
		*s1++ = '\0';
		 DEBUGPRINT (DP_DEBUG, fcomUtilFlag,("*s1++ = 0\n"));
	}
	*lasts = s1;
	 DEBUGPRINT (DP_DEBUG, fcomUtilFlag,("*lasts = %p\n", s1));
	return ret;
}

/******************************************************************************/
/* Name:     fcomParseLCLSPvName
 * Abstract: given a complete LCLS PV name string that conforms to the
 *           LCLS Naming convention, ie DeviceType:Area:Unit:Attribute,
 *           parse its constituent parts by using ":" as a delimiter and return:
 *           deviceType, area, unit, attribute into the contents pointed to by
 *           input char array pointers.
 * Args:     (input) const char * pvName_ca
 *                   pointer to null-terminated array of chars, max length of PV
 * Rem:     Ensure input pvName_ca is null-terminated!
 * 	        Advise zeroing memory prior to populating the pv name
 *
 * Return:  int status  0 for success, -1 for unsuccessful
 *          char * deviceType_ca  - such as "XCOR\0"
 *          char * area_ca        - such as "LTU1\0"
 *          char * unit_ca        - such as "488\0"
 *          char * attrib_ca      - such as "BCTRL\0"
 *          char * slot_ca        - such as "0\0" - "3\0"; Null if not PAU/MUX controlled;
 *                                  also can be passed in as null
 *
 *******************************************************************************/
int fcomParseLCLSPvName (const char * pvName_ca, char * deviceType_ca, char * area_ca, char * unit_ca, char * attrib_ca, char * slot_ca ) {
  int status = 1;   /* success */
  char myName_ca[PVNAME_MAX];
  char * myName_ptr = & (myName_ca[0]);
  char * first_ptr = myName_ptr;
  char * tokenbeg;
  char * tokenend;
  char * delimiter  = { ":" };                /* delimiter string must be null terminated */
  char * slotdelim  = { "_" };                /* delimiter string must be null terminated */
  int sublength = 0;
   
  if (pvName_ca == NULL || deviceType_ca == NULL || area_ca == NULL || unit_ca == NULL || attrib_ca == NULL) {
	  DEBUGPRINT (DP_FATAL, fcomUtilFlag,
	  				("fcomParsePvName: Null pointer passed in! Exiting!\n"));
	  return -1;
  }
  
  strcpy (myName_ca, pvName_ca);
  myName_ca[PVNAME_MAX-1] = '\0';                 /* enforce null terminator */
  tokenbeg = (char *)myName_ca;
  
  DEBUGPRINT (DP_DEBUG, fcomUtilFlag,
  				("fcomParsePvName: Parsing pvName=%s, tokenbeg = %p\n", (char *) myName_ca, tokenbeg));
  /* parse device type */
  /*tokenend = (char *)strtok_r ( &(myName_ca[0]), (const char *)&delimiter , (char **) &myName_ptr);*/
  /*tokenend = (char *)strtok_r ( tokenbeg, (const char *)&delimiter , (char **) &myName_ptr);*/
  /*tokenend = (char *)strtok_r (tokenbeg, ":" , (char **) &myName_ptr);*/
  tokenend = fcomStrtok_r ((char *)first_ptr, (const char *)delimiter , (char **) &myName_ptr);
  if (tokenend == NULL)
	  return -1;
  tokenend =  fcomStrtok_r (NULL, (const char *)delimiter , (char **) &myName_ptr);
   if (tokenend == NULL)
  	  return -1;
  sublength = tokenend - tokenbeg;
  DEBUGPRINT (DP_DEBUG, fcomUtilFlag, ("fcomParsePvName: sublength = %d, tokenend = %p\n", sublength, tokenend));
  strncpy ((char *)deviceType_ca, tokenbeg, sublength);
  deviceType_ca[sublength] = '\0';                      /* add null terminator */
  DEBUGPRINT (DP_DEBUG, fcomUtilFlag,
  				("fcomParsePvName: Parsed deviceType = %s\n", (char *) deviceType_ca));
  /* parse area */
  tokenbeg = tokenend;
  tokenend =  fcomStrtok_r (NULL, (const char *)delimiter , (char **) &myName_ptr);
  if (tokenend == NULL)
 	  return -1;
  sublength = tokenend - tokenbeg;
  strncpy ( (char *)area_ca, tokenbeg, sublength);
  area_ca[sublength] = '\0';                      /* add null terminator */
  DEBUGPRINT (DP_DEBUG, fcomUtilFlag,
  				("fcomParsePvName: Parsed area = %s\n", (char *) area_ca));
  /* parse unit */
  tokenbeg = tokenend;
  tokenend =  fcomStrtok_r (NULL, (const char *)delimiter , (char **) &myName_ptr);
  if (tokenend == NULL)
 	  return -1;
  sublength = tokenend - tokenbeg;
  strncpy ((char *) unit_ca, tokenbeg, sublength);
  unit_ca[sublength] = '\0';                      /* add null terminator */
  DEBUGPRINT (DP_DEBUG, fcomUtilFlag,
  				("fcomParsePvName: Parsed unit = %s\n", (char *) unit_ca));
  /* parse attrib; if PAU controlled device it will have yet another : delimter */
  tokenbeg = tokenend;
  tokenend =  fcomStrtok_r (NULL, (const char *)slotdelim , (char **) &myName_ptr);
  if (tokenend == NULL) {
	  /* parse attrib - not a PAU controlled device */
	  strcpy((char *)attrib_ca, tokenbeg);
	  DEBUGPRINT (DP_DEBUG, fcomUtilFlag,
	   				("fcomParsePvName: Parsed attrib (no PAU slot detected) = %s\n", (char *) attrib_ca));
  } else {
	  /* parse attrib */
	  sublength = tokenend - tokenbeg;
	  DEBUGPRINT (DP_DEBUG, fcomUtilFlag,
			  ("fcomParsePvName: Prior to parsing PAU slot, sublength = %d\n", sublength));
	  strncpy ( (char *)attrib_ca, tokenbeg, sublength);
	  attrib_ca[sublength] = '\0';                      /* add null terminator */
	  DEBUGPRINT (DP_DEBUG, fcomUtilFlag,
			  ("fcomParsePvName: Parsed attrib (w PAU slot detected) = %s\n", (char *) attrib_ca));
	  /* parse PAU slot */
	  /* now see if there is an underscore within the PAU slot to see if it is PAU controlled PV */
	  tokenbeg=tokenend;
	  DEBUGPRINT (DP_DEBUG, fcomUtilFlag,
			  ("fcomParsePvName: Prior to parsing _, tokenend = %p\n", tokenend));
	  tokenend = fcomStrtok_r(NULL, (const char *)slotdelim, (char **) &myName_ptr );
	  if (tokenend==NULL) {
		  DEBUGPRINT (DP_FATAL, fcomUtilFlag,
				  ("fcomParsePvName: slot encoding not found in PV name when expected; Exiting !\n"));
		  return -1;
	  }
	  DEBUGPRINT (DP_DEBUG, fcomUtilFlag,
			  ("fcomParsePvName: After parsing PAU slot, tokenend = %p\n", tokenend));
	  strcpy ((char *)slot_ca, tokenend);
	  DEBUGPRINT (DP_DEBUG, fcomUtilFlag,
	   				("fcomParsePvName: Parsed slot = %s\n", (char *) slot_ca));
  }
  return status;
}
/******************************************************************************/
/* Name:     fcom2GID
 * Abstract: Given a PV's Device type, area, and whether it is a setpoint or
 *           readback, determines the GID. GID (11 bits) is created by:
 *          	FCOM_MASK_<READBACK, SETPOINT> << 10 | device type << 6  | area
 * Args:     (input) const char * deviceType_ptr - for determining device type (such as "XCOR\0")
 *                   const char * area_ptr       - for determining area (such as "LTU1\0")
 *                   const char * attrib_ptr     - for determining setpoint/readback
  * Rem:    Ensure input pointers point to char arrays that are null-terminated!
 * 	        Advise zeroing memory prior to populating the pv name
 *
 * Return:  int status  0 for success, -1 for unsuccessful
 *          GID as type FcomID
 *
 *******************************************************************************/
static FcomID fcom2GID (const char* deviceType_ptr, const char * area_ptr, const char * attrib_ptr )
{
	/* create storage for getting back the parsed device type, area, and attribute strings */
	FcomID fcomid = FCOM_ID_NONE;   /* none found */

    unsigned long invalid = 999999999;

	unsigned long setpoint_mask;
	unsigned long device_mask;
	unsigned long area_mask;
	int i;

	setpoint_mask = 0;          /* assume readback (=0) */
	for (i=0; i<MAX_SETPOINTS; i++) {
		if (strcmp ( (char *)(setpoint_ca[i]), attrib_ptr) == 0 ) {
			/* this is a setpoint */
			setpoint_mask = 1;
			DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2GID: setpoint found: %s\n", (char *)(setpoint_ca[i]) ));
			break;
		}
	}
	if (setpoint_mask==0){
		DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2GID: readback (is default, otherwise INVALID Attribute) found: %s\n", (char *)attrib_ptr));
	}

	device_mask = invalid;
	for (i=0; i<MAX_DEVTYPES; i++) {
		if ( strcmp ( (char *)(devtype_ca[i]), deviceType_ptr) == 0 ) {
			/* this is a match for deviceType */
			device_mask = FCOM_MASK_DEVICE_START + i;
			DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2GID: mask= 0x%lx for device: %s\n", device_mask, (char *)(devtype_ca[i]) ));
			break;
		}
	}
		
	area_mask = FCOM_MASK_DEVICE_START;  
	if (device_mask != invalid) {
		for ( i=0; i<MAX_AREAS; i++) {
			if ( strcmp ( (char *)(area_ca[i]), area_ptr) == 0 ) {
				/* this is a match for area */
				area_mask = FCOM_MASK_AREA_START + i;
				DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2GID: mask= 0x%lx for area: %s\n", area_mask, (char *)(area_ca[i]) ));
				break;
			}
		}
	}


	if (device_mask == invalid) {
		DEBUGPRINT(DP_INFO,fcomUtilFlag, ("fcom2GID: invalid device mask for device %s\n", (char*) deviceType_ptr ));
	} else {
		/*  Create (16bit) GID from pv name string using 	FCOM_MASK_<READBACK, SETPOINT> << 10 | dev << 6  | area */
		/* if attib_ptr contains an actuator setpoint (BCTRL, L0B_A, L1S_P, L1S_A, P, A), then set setpoint bit
		 */
		fcomid = ( setpoint_mask << 10) | (device_mask << 6) | area_mask;
		DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2GID: GID = 0x%lx (setpoint_mask<<10)|(device_mask<<6)|area_mask\n",
				(unsigned long) fcomid ));
	}
	if ( (fcomid >= FCOM_GID_MIN) && (fcomid <= FCOM_GID_MAX))
	  return fcomid;
	else return FCOM_ID_NONE;
								    
}
/******************************************************************************/
/* Name:     fcom2SID
 * Abstract: Given a PV's Device type, area, unit, attribute, and slot ID,
 *           determines the SID. SID (16 bits) is created by:
 *          	FCOM_MASK_<READBACK, SETPOINT> << 10 | device type << 6  | area
 * Args:     (input) const char * deviceType_ptr - for determining device type (such as "XCOR\0")
 *                   const char * area_ptr       - for determining area (such as "LTU1\0")
 *                   const char * unit_ptr       - for determining unit (such as "100\0")
 *                   const char * attrib_ptr     - for determing attribute (such as "X\0")
 *                   const char * slot_ptr       - if PAU PV
  * Rem:    Ensure input pointers point to char arrays that are null-terminated!
 * 	        Advise zeroing memory prior to populating the pv name
 *
 * Return:  int status  0 for success, -1 for unsuccessful
 *          SID returned as type FcomID
 *
 *******************************************************************************/
static FcomID fcom2SID ( const char* deviceType_ptr, const char * area_ptr, const char * unit_ptr,
		const char * attrib_ptr, const char * slot_ptr )
{
	char rfDevName_ca[PVNAME_MAX];
	unsigned short found = 0;
	int i;
	int slot = 0;
	
	/* create storage for getting back the parsed device type, area, and attribute strings */
	FcomID fcomid = FCOM_ID_NONE;   /* none found */

	/* determine if this is a detector, rf, magnet, or fbck state */
	for (i=0; i<MAX_DETECTOR_NAMES; i++) {
		if ( strcmp ( (char *)(detector_ca[i]), deviceType_ptr) == 0 ) {
			/* this is a detector */
			/* FcomID is unit #   */
			DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2SID: detector found: %s\n", (char *)(detector_ca[i]) ));
			int unit = atoi(unit_ptr);
			fcomid = (FcomID) unit;
			found = 1;
			DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2SID: DETECTOR- (unit), where unit =%d\n", unit ));
			break;
		}
	}
	if (! found) {
		if (strcmp(deviceType_ptr, "FBCK")==0) {
			/* feedback state */
			DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2SID: FBCK found\n" ));
			/* FcomID is loop #, where loop #=LoopType (2bits) <<3 | LoopNum (3bits) */
			/* all loop info is bundled together */
			int looptype, loopnum;
			const char * loopnum_ptr = unit_ptr;
			loopnum_ptr += 2;
			
			for (i=0; i<MAX_LOOP_TYPES; i++) {
				if ( strncmp ( (char *)(looptype_ca[i]), unit_ptr, 2) == 0 ) {
					DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2SID: looptype found fir %s; assigned # %d\n", (char *)(looptype_ca[i]), i ));
					looptype = i;
					found = 1;
					loopnum = atoi(loopnum_ptr);
					fcomid = (looptype << 3) | loopnum;
					DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2SID: FBCK- (looptype << 3)| loopnum, where looptype=%d, loopnum=%d\n", looptype, loopnum ));
					break;
				}
			}
		} else if (strncmp((const char *)(deviceType_ptr+1), "COR", 3)==0) { 
			/* xcor or ycor magnet */
			int unit = atoi(unit_ptr);
			DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2SID: Magnet - unit=%d\n", unit));
			/* strrchr() returns a pointer to the last occurrence of '_' in str, or NULL if no match is found. */
			if (slot_ptr != NULL) {
				slot = atoi(slot_ptr);
				DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2SID: Magnet - slot=%d\n", slot));
				if (slot < MAX_PAU_SLOTS ) {
					fcomid = (unit << MAX_PAU_SLOTS) | slot;
				}
				DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2SID: Magnet - (unit << 4) | slot, where unit=0x%x, slot=0x%x\n", unit, slot));
			}

		} else {
			/* llrf device */
			sprintf ((char *)rfDevName_ca, "%s:%s:%s:%s", deviceType_ptr,area_ptr, unit_ptr,attrib_ptr);
			DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2SID: Checking for LLRF device %s...\n", (char *) rfDevName_ca ));
			for (i=0; i<MAX_RFNAMES; i++) {
				if ( strcmp((char *)(rfname_ca[i]),(char *)rfDevName_ca) == 0 ) {
	                DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2SID: Match found for index %d, device %s...\n", i, (char *) rfname_ca[i] ));
					/* this is a match for rfName */
					unsigned long device_mask = FCOM_MASK_SIG_START + i;
					if (slot_ptr != NULL) {
						slot = atoi(slot_ptr);
						if (!(slot < MAX_PAU_SLOTS )) 
							slot = 0;
					}	
					fcomid = (device_mask << MAX_PAU_SLOTS) | slot;
					DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcom2SID: LLRF - (unique llrf sig id << 4) | slot, where id=0x%lx, slot=%d\n", 
							device_mask, slot));
					break;
				}
			}
		}
	}
    if (fcomid !=  FCOM_ID_NONE)
	  fcomid += FCOM_SID_MIN;
	if ( (fcomid >= FCOM_SID_MIN) && (fcomid <= FCOM_SID_MAX))
	  return fcomid;
	else return FCOM_ID_NONE;

}
/******************************************************************************/
/* Name:     fcomLCLSPV2FcomID
 * Abstract: given a pointer to a null-terminated LCLS PV name string (array of char)
 *           that conforms to the LCLS Naming convention, ie DeviceType:Area:Unit:Attribute,
 *           parse its constituent parts (separated by ":") by using fcomParsePvName,
 *           then call fcom2GID and fcom2SID to get GID and SID, respectively.
 *           Finally, form the FcomID from the GID and SID using FCOM utility macro.
 * Args:     (input) const char * pvName_ptr
 *
 * Rem:    Ensure input pointers point to char arrays that are null-terminated!
 * 	        Advise zeroing memory prior to populating the pv name
 *
 * Return:  int status  0 for success, -1 for unsuccessful
 *          FCOM ID as type FcomID
 *
 *******************************************************************************/
FcomID fcomLCLSPV2FcomID (const char* pvName_ptr )
{
	FcomID fcomid = FCOM_ID_NONE;   /* none found */
	FcomID gid, sid = FCOM_ID_NONE;

    /*  Create (16bit) GID from pv name string using 	FCOM_MASK_<READBACK, SETPOINT> << 10 | dev << 6  | area */

	/* create storage for returned parsed strings */
	char deviceType_ca[PVNAME_MAX];
	char area_ca [PVNAME_MAX];
	char unit_ca [PVNAME_MAX];
	char attrib_ca[PVNAME_MAX];
	char slot_ca[PVNAME_MAX];

	/* parse full name into components  */
	if (fcomParseLCLSPvName (pvName_ptr, (char *)(deviceType_ca),  (char *)(area_ca),  (char *) (unit_ca), 
			 (char *)(attrib_ca),  (char *)(slot_ca) ) )  {
		/* get GID */
		gid = fcom2GID ( (const char*)(deviceType_ca), (const char *)(area_ca), (const char *)(attrib_ca) );
		DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcomPV2FcomID: GID= 0x%lx for signal: %s\n",(unsigned long) gid, (char *)pvName_ptr));
		/* get SID */
		if (gid != FCOM_ID_NONE) {
			sid = fcom2SID ((const char*) (deviceType_ca), (const char *)(area_ca), (const char *)(unit_ca),
					(const char *)(attrib_ca), (const char *)(slot_ca) );
			DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcomPV2FcomID: SID= 0x%lx for signal: %s\n",(unsigned long) sid, (char *)pvName_ptr));
			/* get fcomid */
			if (sid != FCOM_ID_NONE) {
			  fcomid = FCOM_MAKE_ID(gid,sid);
			  DEBUGPRINT(DP_INFO,fcomUtilFlag, ("fcomPV2FcomID: GID=0x%lx; SID=0x%lx; FcomID= 0x%lx for signal: %s\n",
					(unsigned long) gid, (unsigned long) sid, (unsigned long) fcomid, (char *)pvName_ptr));
			}
			else { 
			  fcomid = FCOM_ID_NONE;
			  DEBUGPRINT(DP_INFO,fcomUtilFlag, ("fcomPV2FcomID: FCOM_ID_NONE ... Invalid SID for signal for signal: %s\n",
					(char *)pvName_ptr));
			}
		} else {
		  fcomid = FCOM_ID_NONE;
		  DEBUGPRINT(DP_INFO,fcomUtilFlag, ("fcomPV2FcomID: FCOM_ID_NONE ... Invalid GID for signal: %s\n",
					(char *)pvName_ptr));
		}
	}
	return fcomid;
}

/*=============================================================================
 
  Name: fcomUtilGetFcomID

  Abs:  The function returns the pv's FcomID.
  
  Args: char * pv name string
 
  Rem:  Debug routine to verify GID, SID, FcomID. Used from iocSh too.

  Side: 
. 
  Ret:  none
  
=============================================================================*/
void fcomUtilGetFcomID (const char * pvNameString)
{
  FcomID            fcomid;  
  
  /*printf ("fcomUtilGetFcomID reached\n");*/
  
  if (pvNameString == NULL ) {
          printf("ERROR: fcomUtilGetFcomID has no PV name; Exiting! \n");       
          return;
  }
  
  if (!(strlen (pvNameString)>6 ) ) {
          printf ("ERROR: fcomUtilGetFcomID has illegal PV name; Exiting! \n");         
          return;
  }       
  
  DEBUGPRINT(DP_DEBUG,fcomUtilFlag, ("fcomUtilGetFcomID reached for pv name=%s \n", pvNameString));
 
  fcomid = fcomLCLSPV2FcomID (pvNameString );
  if (fcomid > FCOM_ID_NONE)
          printf ("FcomID=0x%lx for signal=%s\n", (unsigned long) fcomid, pvNameString);
  else printf ("No FcomID found, sorry \n");
  
}
