//////////////////////////////////////////////////////////////////////////////
// This file is part of 'fcomUtil'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'fcomUtil', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef FCOM_LCLS_BPM_H
#define FCOM_LCLS_BPM_H
/* $Id$ */

/* Header defining the layout of a BPM FCOM blob
 * as well as status values.
 */

/*
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2/2009
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Always use FcblBpmNumber -- if the data type is
 * ever changed then this change can be propagated
 * simply by redefining this typedef.
 */
typedef float FcblBpmNumber;

/* Macros to access BPM Array fields; use as follows:
 *
 * FcomBlobRef p_bpmpos;
 *
 *   fcomGetBlob(id, &p_bpmpos, 0);
 *
 *   FcblBpmNumber tmit = p_bpmpos->fcbl_bpm_T;
 *
 * ALWAYS access data using these macros
 * so that changes to the layout and/or data
 * type can be propagated by recompiling.
 */
#define fcbl_bpm_X fc_flt[0]
#define fcbl_bpm_Y fc_flt[1]
#define fcbl_bpm_T fc_flt[2]

/* Status values (other values not defined here must
 * be interpreted as 'all data invalid').
 */
#define FC_STAT_BPM_OK	         0

/* Reference too small; T value may be used but
 * X and Y are INVALID/UNDEFINED.
 */
#define FC_STAT_BPM_REFLO        1

/*
 * One or more digitizer channels detected an overrange
 * condition. Data (all values X/Y/T) may be imprecise
 * with an undefined error margin.
 */
#define FC_STAT_BPM_OVRFL        2

/*
 * Some parameter was set to a bad value so that
 * position could not be calculated. T may be used
 * but X/Y may be imprecise with an undefined
 * error margin.
 */
#define FC_STAT_BPM_PVIOL        3

/*
 * Bpm position couldn't be measured (hardware, network
 * or software error). All data are INVALID/UNDEFINED. 
 */
#define FC_STAT_BPM_INVAL    0xffff

#ifdef __cplusplus
}
#endif

#endif
