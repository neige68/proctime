@rem <install.bat> -*- coding: cp932-dos -*-
@echo off
rem
rem Project proctime
rem Copyright (C) 2023 neige68
rem
rem Note: install
rem
rem Windows:   XP and lator
rem

setlocal

replace build\release\proctime.exe %LOCALBIN%\ /a
replace build\release\proctime.exe %LOCALBIN%\ /u

@rem end of <install.bat>
