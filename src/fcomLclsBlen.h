//////////////////////////////////////////////////////////////////////////////
// This file is part of 'fcomUtil'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'fcomUtil', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef FCOM_LCLS_BLEN_H
#define FCOM_LCLS_BLEN_H

#ifdef __cplusplus
extern "C" {
#endif


/* Macros to access BLEN Array fields; use as follows:
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

/* Status values
 * The IMAX value is invalid when any of the following is true:
 *  (1) tmit did not arrive
 *  (2) tmit value is too small
 *  (3) the raw value is too small
 * Since there are two imax values, we will use a bitmap for the error code
 *  bit 0 -- aimax
 *  bit 1 -- bimax
 * An error exists when the corresponding bit in the status is set.
 */
#define FC_STAT_BLEN_OK                0
#define FC_STAT_BLEN_INVAL_AIMAX_MASK  1  
#define FC_STAT_BLEN_INVAL_BIMAX_MASK  2

#ifdef __cplusplus
}
#endif

#endif
