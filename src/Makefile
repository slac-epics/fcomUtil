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

LIBRARY_IOC = fcomUtil
PROD_IOC = pv2fcid


fcomUtil_SRCS += fcomUtilRegister.cpp
fcomUtil_SRCS += fcomUtil.c
fcomUtil_LIBS += fcomBSD
fcomUtil_LIBS += miscUtils
fcomUtil_LIBS := $(EPICS_BASE_IOC_LIBS)

pv2fcid_SRCS = pv2fcid.c
pv2fcid_LIBS = fcomUtil
pv2fcid_LIBS += $(EPICS_BASE_IOC_LIBS)
#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

