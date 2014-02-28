#ifndef FCOM_LCLS_LLRF_H
#define FCOM_LCLS_LLRF_H

#ifdef __cplusplus
extern "C" {
#endif


/* Macros to access LLRF Array fields; use as follows:
 *
 * FcomBlobRef p_llrfData;
 *
 *   fcomGetBlob(id, &p_llrfData, 0);
 *
 *   float pavg = p_llrfData->fcbl_llrf_pavg;
 *   float aavg = p_llrfData->fcbl_llrf_aavg;
 */
#define fcbl_llrf_pavg fc_flt[0]
#define fcbl_llrf_aavg fc_flt[1]

#define FC_STAT_LLRF_OK                0
#define FC_STAT_LLRF_INVAL_PAVG_MASK  1  
#define FC_STAT_LLRF_INVAL_AAVG_MASK  2

#ifdef __cplusplus
}
#endif

#endif
