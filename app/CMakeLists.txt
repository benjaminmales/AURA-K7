# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Force use of C11 standard
set(CMAKE_C_STANDARD 11)
set_property(GLOBAL PROPERTY CSTD c11)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(kkm_demo)

target_sources(app PRIVATE src/main.c)
