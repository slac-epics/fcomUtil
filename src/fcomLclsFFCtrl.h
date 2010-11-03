#ifndef FCOM_LCLS_FFCTRL_H
#define FCOM_LCLS_FFCTRL_H
/* $Id: fcomLclsFFCtrl.h,v 1.1 2010/08/11 19:49:52 dfairley Exp $ */

/* Header defining the layout of a Controller FCOM blobs
 * as well as status values.
 */

/*
 * Author: D. Fairley <dfairley@slac.stanford.edu>, 2/2009
 */

#ifdef __cplusplus
extern "C" {
#endif

/************ FFController feedback loop Actuator commands ****************/
/*
 * The feedback loop controller sends actuator commands as single messages 
 * directly to each actuator. Therefore there is no need to define a layout
 * for actuator command messages.
 *
 * also, at this time the feedback loop controller will not include any status
 * in it's actuator command messages, instead if there is any problem with the
 * command it simply will not send it. Therefore there is no need to define 
 * status values for these messages - they will always include the FC_STAT_FF_OK value.
 */
/* Status values (other values not defined in this file must
 * be interpreted as 'all data invalid').
 */
#define FC_STAT_FF_OK	         0

/************ FFController feedback loop STATE messages ****************/
/*
 * Always use FcblStateNumber -- if the data type is
 * ever changed then this change can be propagated
 * simply by redefining this typedef.
 */
typedef float FcblStateNumber;

/* Macros to access FFCtrl State Array fields; use as follows:
 *
 * FcomBlobRef p_ffState;
 *
 *   fcomGetBlob(id, &p_ffStates, 0);
 *
 *   FcblStateNumber xposition = p_ffState->fcbl_ffState_xpos;
 *
 * ALWAYS access data using these macros
 * so that changes to the layout and/or data
 * type can be propagated by recompiling.
 */
/* transverse feedbacks */
#define fcbl_ffState_xpos   fc_flt[0]
#define fcbl_ffState_ypos   fc_flt[1]
#define fcbl_ffState_xang   fc_flt[2]
#define fcbl_ffState_yang   fc_flt[3]

/* longitudinal feedback */
#define fcbl_ffState_dl1e   fc_flt[0] /* dog-leg 1 energy */
#define fcbl_ffState_bc1e   fc_flt[1] /* BC1 energy */
#define fcbl_ffState_bc1i   fc_flt[2] /* BC1 current */
#define fcbl_ffState_bc2e   fc_flt[3] /* BC2 energy */
#define fcbl_ffState_bc2i   fc_flt[4] /* BC2 current */
#define fcbl_ffState_dl2e   fc_flt[5] /* dog-leg 2 energy */

/* generic feedbacks  define up to 10 states max*/
#define fcbl_ffState_s1     fc_flt[0] 
#define fcbl_ffState_s2     fc_flt[1] 
#define fcbl_ffState_s3     fc_flt[2] 
#define fcbl_ffState_s4     fc_flt[3] 
#define fcbl_ffState_s5     fc_flt[4] 
#define fcbl_ffState_s6     fc_flt[5] 
#define fcbl_ffState_s7     fc_flt[6] 
#define fcbl_ffState_s8     fc_flt[7] 
#define fcbl_ffState_s9     fc_flt[8] 
#define fcbl_ffState_s10    fc_flt[9] 

/* For now, the status word for feedback States messages is a bitmap - this limits
 * the number of possible array elements to 16 states, but it should be enough for any
 * single feedback
 *
 * bit-map:individual array entry is INVALID 
 * ex: blob.fc_stat = FC_STAT_BC1E_INVAL & FC_STAT_DL2E_INVAL; 
 * ex: if !(p_ffState->hdr.stat && FC_STAT_XPOS_INVAL) xang = p_ffState->fcbl_ffState_xang
 */
#define FC_STAT_FF1_INVAL       0x0001
#define FC_STAT_FF2_INVAL       0x0002
#define FC_STAT_FF3_INVAL       0x0004
#define FC_STAT_FF4_INVAL       0x0008
#define FC_STAT_FF5_INVAL       0x0010
#define FC_STAT_FF6_INVAL       0x0020
#define FC_STAT_FF7_INVAL       0x0040
#define FC_STAT_FF8_INVAL       0x0080
#define FC_STAT_FF9_INVAL       0x0100
#define FC_STAT_FF10_INVAL      0x0200

#define FC_STAT_XPOS_INVAL  FC_STAT_FF0_INVAL       
#define FC_STAT_YPOS_INVAL  FC_STAT_FF1_INVAL       
#define FC_STAT_XANG_INVAL  FC_STAT_FF2_INVAL       
#define FC_STAT_XANG_INVAL  FC_STAT_FF3_INVAL      

#define FC_STAT_DL1E_INVAL  FC_STAT_FF0_INVAL       
#define FC_STAT_BC1E_INVAL  FC_STAT_FF1_INVAL       
#define FC_STAT_BC1I_INVAL  FC_STAT_FF2_INVAL       
#define FC_STAT_BC2E_INVAL  FC_STAT_FF3_INVAL      
#define FC_STAT_BC2I_INVAL  FC_STAT_FF4_INVAL       
#define FC_STAT_DL2E_INVAL  FC_STAT_FF5_INVAL       

/*
 * All data are INVALID/UNDEFINED. 
 */
#define FC_STAT_FF_INVAL    0xffff

#ifdef __cplusplus
}
#endif

#endif
