# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

# add_subdirectory(atomic)  # header only
add_subdirectory(atomic)
add_subdirectory(clog)
add_subdirectory(ghost)
add_subdirectory(guardedalloc)
add_subdirectory(libmv)
add_subdirectory(memutil)
add_subdirectory(opencolorio)
add_subdirectory(opensubdiv)
add_subdirectory(mikktspace)
add_subdirectory(glew-mx)
add_subdirectory(eigen)
add_subdirectory(sky)

if(WITH_AUDASPACE)
  add_subdirectory(audaspace)
endif()

if(WITH_MOD_REMESH)
  add_subdirectory(dualcon)
endif()

if(WITH_IK_SOLVER)
  add_subdirectory(iksolver)
endif()

if(WITH_IK_ITASC)
  add_subdirectory(itasc)
endif()

if(WITH_CYCLES)
  add_subdirectory(cycles)
endif()

if(WITH_INTERNATIONAL)
  add_subdirectory(locale)
endif()

if(WITH_BULLET)
  add_subdirectory(rigidbody)
endif()

# only windows needs utf16 converter
if(WIN32)
  add_subdirectory(utfconv)
endif()

if(WITH_MOD_FLUID)
  add_subdirectory(mantaflow)
endif()

if(WITH_OPENVDB)
  add_subdirectory(openvdb)
endif()

if(WITH_QUADRIFLOW)
  add_subdirectory(quadriflow)
endif()

if(WITH_CODEC_FFMPEG)
  add_subdirectory(ffmpeg)
endif()

if(UNIX AND NOT APPLE)
  add_subdirectory(libc_compat)
endif()
