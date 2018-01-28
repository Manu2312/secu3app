@echo off
rem SECU-3  - An open source, free engine control unit
rem Copyright (C) 2007 Alexey A. Shabelnikov. Ukraine, Kiev
rem 
rem Batch file for generating check sum for firmware of SECU-3 project
rem Created by Alexey A. Shabelnikov, Kiev 26 September 2009. 

set HEXTOBIN=hextobin.exe
set CODECRC=codecrc.exe
set USAGE=Supported options: M64,M644,M1284
set FW_SIZE=Undefined
set CRC_ADDR=Undefined

IF "%1" == "" (
echo Command line option required.
echo %USAGE%
exit 1
)

rem Check validity of command line option and set corresponding parameters
IF %1 == M64 ( 
set FW_SIZE=63486
set CRC_ADDR=F7FE
GOTO dowork
)

IF %1 == M644 ( 
set FW_SIZE=63486
set CRC_ADDR=F7FE
GOTO dowork
)

IF %1 == M1284 ( 
set FW_SIZE=129022
set CRC_ADDR=1F7FE
GOTO dowork
)

echo Invalid platform! 
echo %USAGE%
exit 1

:dowork
echo EXECUTING BATCH...
echo ---------------------------------------------

rem Convert HEX-file created by compiler into a binary file
for %%X in (%HEXTOBIN%) do (set FOUND_H2B=%%~$PATH:X)
if not defined FOUND_H2B (
 echo ERROR: Can not find file "%HEXTOBIN%"
 goto error
)
%HEXTOBIN% secu-3_app.a90 secu-3_app.bin
IF ERRORLEVEL 1 GOTO error

rem Make a copy of file which doesn't contain check sum
rem copy secu-3_app.bin secu-3_app0000.bin
rem copy secu-3_app.a90 secu-3_app0000.a90

rem Calculate and put check sum into a binary file
for %%X in (%CODECRC%) do (set FOUND_CRC=%%~$PATH:X)
if not defined FOUND_CRC (
 echo ERROR: Can not find file "%CODECRC%"
 goto error
)
%CODECRC% secu-3_app.bin secu-3_app.a90  0  %FW_SIZE%  %CRC_ADDR% -h
IF ERRORLEVEL 1 GOTO error
%CODECRC% secu-3_app.bin secu-3_app.bin  0  %FW_SIZE%  %CRC_ADDR% -b
IF ERRORLEVEL 1 GOTO error

echo ---------------------------------------------
echo ALL OPERATIONS WERE COMPLETED SUCCESSFULLY!
exit 0

:error
echo ---------------------------------------------
echo WARNING! THERE ARE SOME ERRORS IN EXECUTING BATCH.
exit 1
