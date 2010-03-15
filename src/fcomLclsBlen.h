#ifndef FCOM_LCLS_BLEN_H
#define FCOM_LCLS_BLEN_H

#ifdef __cplusplus
extern "C" {
#endif


/* Macros to access BPM Array fields; use as follows:
 *
 * FcomBlobRef p_blenData;
 *
 *   fcomGetBlob(id, &p_blenData, 0);
 *
 *   float araw = p_blenData->fcbl_blen_araw;
 */
#define fcbl_blen_araw  fc_flt[0]
#define fcbl_blen_aimax fc_flt[1]
#define fcbl_blen_braw  fc_flt[2]
#define fcbl_blen_bimax fc_flt[3]


#ifdef __cplusplus
}
#endif

#endif
