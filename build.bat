@rem <build.bat> -*- coding: cp932-dos -*-
@echo off
rem
rem Project proctime
rem Copyright (C) 2023 neige68
rem
rem Note: build
rem
rem Windows:   XP and lator
rem

setlocal
pushd %~dp0
set @exec_cmake=

rem VC の vcvarsall.bat のあるディレクトリを指定
set VC=%VC142%
set CMAKEOPT=-G "Visual Studio 16 2019" -A Win32 

:optloop
if "%1"=="" goto optend
if "%1"=="cm" set @exec_cmake=t
if "%1"=="cm" goto optnext
if "%1"=="re" rmdir /q /s build
if "%1"=="re" goto optnext
echo WARN: build.bat: オプション %1 が無効です.
:optnext
shift
goto optloop
:optend

if "%VCToolsVersion%"=="" call "%VC%\vcvarsall.bat" x86
if not exist build mkdir build
pushd build
if not exist ALL_BUILD.vcxproj set @exec_cmake=t
if not "%@exec_cmake%"=="" cmake %CMAKEOPT% ..
msbuild ALL_BUILD.vcxproj /p:Configuration=Debug /m
echo INFO: build.bat: msbuild Debug Done.
if errorlevel 1 goto err
ctest -C debug -j %NUMBER_OF_PROCESSORS% --output-on-failure --timeout 5
if errorlevel 1 goto err
msbuild ALL_BUILD.vcxproj /p:Configuration=Release /m
echo INFO: build.bat: msbuild Release Done.
:err
popd
:err_pop1
popd
if errorlevel 1 echo エラーがあります.
if errorlevel 1 exit /b 1
rem end of build.bat
