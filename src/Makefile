TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#=============================
# Build the IOC support library

# for gdb/ddd debugger:
USR_CFLAGS_linux-x86 += -g
# for debug print statements 
USR_CFLAGS += -DDEBUG_PRINT

INC += fcomUtil.h
INC += fcomLclsBpm.h
INC += fcomLclsBlen.h
INC += fcomLclsLlrf.h
INC += fcomLclsFFCtrl.h

DBD += fcomUtil.dbd
DBD += devFcom.dbd

LIBRARY_IOC = fcomUtil
PROD_IOC    = pv2fcid

fcomUtil_SRCS += fcomUtilRegister.cpp
fcomUtil_SRCS += fcomUtil.c
fcomUtil_SRCS += devFcom.c
fcomUtil_SRCS += devFcomSubRecord.c
fcomUtil_SRCS += drvFcom.c
fcomUtil_LIBS += fcomBSD
fcomUtil_LIBS += miscUtils
fcomUtil_LIBS := $(EPICS_BASE_IOC_LIBS)

pv2fcid_SRCS = pv2fcid.c fcomUtil.c
pv2fcid_LIBS += $(EPICS_BASE_IOC_LIBS)
#=============================
# Install the FCOM DTYP templates and samples

DB	+=	fcomBlobAi.template
DB	+=	fcomBlobAo.template
DB	+=	fcomBlobAdd.template
DB	+=	fcomBlobSet.template
DB	+=	fcomBlobSim.template
DB	+=	fcomGroupSend.template
DB	+=	fcomBlobBPMS.db
DB	+=	fcomBlobBLEN_SIM.db
DB	+=	fcomBlobBLEN_SIM.substitutions
DB	+=	fcomBlobBPMS_SIM.db
DB	+=	fcomBlobBPMS_SIM.substitutions
DB	+=	fcomBlobFBCK_SIM.db
DB	+=	fcomBlobFBCK_SIM.substitutions
DB	+=	fcomSampleSim.db
DB	+=	fcomSampleSim.substitutions

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

