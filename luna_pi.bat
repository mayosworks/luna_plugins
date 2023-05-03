@echo off
rem ===========================================================================
rem LunaPlugin Header copy utility
rem                                               Copyright (c) 2006-2015 MAYO.
rem ===========================================================================

@echo on
pause Copy "luna_pi.h" to each folder?
for /f %%i in ('dir /a:d /b') do copy luna_pi.h .\%%i /y
pause
