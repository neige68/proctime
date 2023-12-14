# <CMakeLists.txt> -*- coding: utf-8 -*-
#
# Project proctime
# Copyright (C) 2023 neige68
#

cmake_minimum_required(VERSION 3.24)
set(CMAKE_VERBOSE_MAKEFILE ON)
# スタティックリンク
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(proctime CXX)

#------------------------------------------------------------
#
# boost
#

# BOOST を探す
# C: ドライブ以外から探すには 環境変数 CMAKE_PREFIX_PATH の設定が必要
find_package(Boost 1.81 REQUIRED COMPONENTS filesystem)
message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

#------------------------------------------------------------

# C++17 使用
set(CMAKE_CXX_STANDARD "17")

# /MP: マルチプロセス
# /W4: 警告レベルを4とする
# /Yu: プリコンパイルヘッダを使用
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /W4 /Yu\"pch.h\"")

# マップファイル生成
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MAP")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MAP")

# ローカルインクルードファイルの場所を追加
include_directories($ENV{LOCALINCLUDE})

add_compile_definitions(_CRT_RAND_S)

add_executable(proctime proctime.cpp pch.cpp)
# プリコンパイルヘッダを生成
set_source_files_properties(pch.cpp PROPERTIES COMPILE_FLAGS "/Yc\"pch.h\"")

# end of <CMakeLists.txt>
