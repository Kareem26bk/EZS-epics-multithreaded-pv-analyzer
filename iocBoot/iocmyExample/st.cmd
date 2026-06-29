#!../../bin/darwin-aarch64/myExample

#- You may have to change myExample to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/myExample.dbd"
myExample_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadTemplate "db/user.substitutions"
dbLoadRecords "db/myExampleVersion.db", "user=abal8593"
dbLoadRecords "db/dbSubExample.db", "user=abal8593"

#- Set this to see messages from mySub
#-var mySubDebug 1

#- Run this to trace the stages of iocInit
#-traceIocInit

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncExample, "user=abal8593"
