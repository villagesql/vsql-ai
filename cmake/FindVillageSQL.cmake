# Copyright (c) 2026 VillageSQL Contributors
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#[=======================================================================[.rst:
FindVillageSQL
--------------

Find the VillageSQL Extension SDK.

This module finds the VillageSQL Extension SDK using the following methods
(in order of priority):

1. ``VillageSQL_SDK_DIR`` if explicitly set by the user
2. The ``villagesql_config`` script if found in PATH
3. Direct detection in ``~/.villagesql``

Imported Targets
^^^^^^^^^^^^^^^^

This module does not define imported targets directly. Instead, it locates
the SDK and delegates to ``VillageSQLExtensionFrameworkConfig.cmake`` which
provides the ``VEF_CREATE_VEB()`` function for building extensions.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``VillageSQL_FOUND``
  True if the SDK was found.

``VillageSQL_VERSION``
  The version of the SDK found (if available).

``VillageSQL_PREFIX``
  The installation prefix of the SDK.

``VillageSQL_INCLUDE_DIR``
  The include directory for SDK headers.

``VillageSQL_CXX_FLAGS``
  Compiler flags for building extensions.

``VILLAGESQL_CONFIG_EXECUTABLE``
  Path to the villagesql_config script (if found).

Cache Variables
^^^^^^^^^^^^^^^

``VillageSQL_SDK_DIR``
  Set this variable to the SDK installation directory to override
  automatic detection.

Requirements
^^^^^^^^^^^^

The VillageSQL Extension SDK requires C++17 or later. This module will
check that ``CMAKE_CXX_STANDARD`` is set appropriately.

Example Usage
^^^^^^^^^^^^^

::

  # Automatic detection
  find_package(VillageSQL REQUIRED)

  # Or with explicit path
  cmake -DVillageSQL_SDK_DIR=/path/to/sdk ..

#]=======================================================================]

set(_villagesql_found FALSE)

# Method 1: Use VillageSQL_SDK_DIR if explicitly set
if(VillageSQL_SDK_DIR)
  if(EXISTS "${VillageSQL_SDK_DIR}/include/villagesql/extension.h")
    set(VillageSQL_PREFIX "${VillageSQL_SDK_DIR}")
    set(VillageSQL_INCLUDE_DIR "${VillageSQL_SDK_DIR}/include")
    set(VillageSQL_CXX_FLAGS "-I${VillageSQL_SDK_DIR}/include")
    # Try to get version from villagesql_config if present
    if(EXISTS "${VillageSQL_SDK_DIR}/bin/villagesql_config")
      set(VILLAGESQL_CONFIG_EXECUTABLE "${VillageSQL_SDK_DIR}/bin/villagesql_config")
      execute_process(
        COMMAND ${VILLAGESQL_CONFIG_EXECUTABLE} --version
        OUTPUT_VARIABLE VillageSQL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
    endif()
    set(_villagesql_found TRUE)
  else()
    message(WARNING
      "VillageSQL_SDK_DIR is set to '${VillageSQL_SDK_DIR}' "
      "but SDK headers were not found there."
    )
  endif()
endif()

# Method 2: Look for villagesql_config in PATH
if(NOT _villagesql_found)
  find_program(VILLAGESQL_CONFIG_EXECUTABLE
    NAMES villagesql_config
    DOC "Path to villagesql_config script"
  )

  if(VILLAGESQL_CONFIG_EXECUTABLE)
    execute_process(
      COMMAND ${VILLAGESQL_CONFIG_EXECUTABLE} --version
      OUTPUT_VARIABLE VillageSQL_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

    execute_process(
      COMMAND ${VILLAGESQL_CONFIG_EXECUTABLE} --prefix
      OUTPUT_VARIABLE VillageSQL_PREFIX
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

    execute_process(
      COMMAND ${VILLAGESQL_CONFIG_EXECUTABLE} --include
      OUTPUT_VARIABLE VillageSQL_INCLUDE_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

    execute_process(
      COMMAND ${VILLAGESQL_CONFIG_EXECUTABLE} --cxxflags
      OUTPUT_VARIABLE VillageSQL_CXX_FLAGS
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

    if(VillageSQL_PREFIX AND VillageSQL_INCLUDE_DIR)
      set(_villagesql_found TRUE)
    endif()
  endif()
endif()

# Method 3: Fall back to ~/.villagesql
if(NOT _villagesql_found)
  set(_default_prefix "$ENV{HOME}/.villagesql")

  if(EXISTS "${_default_prefix}/include/villagesql/extension.h")
    set(VillageSQL_PREFIX "${_default_prefix}")
    set(VillageSQL_INCLUDE_DIR "${_default_prefix}/include")
    set(VillageSQL_CXX_FLAGS "-I${_default_prefix}/include")
    # Try to get version from villagesql_config if present
    if(EXISTS "${_default_prefix}/bin/villagesql_config")
      set(VILLAGESQL_CONFIG_EXECUTABLE "${_default_prefix}/bin/villagesql_config")
      execute_process(
        COMMAND ${VILLAGESQL_CONFIG_EXECUTABLE} --version
        OUTPUT_VARIABLE VillageSQL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
    endif()
    set(_villagesql_found TRUE)
  endif()

  unset(_default_prefix)
endif()

unset(_villagesql_found)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VillageSQL
  REQUIRED_VARS
    VillageSQL_PREFIX
    VillageSQL_INCLUDE_DIR
  VERSION_VAR VillageSQL_VERSION
)

if(VillageSQL_FOUND)
  # Check C++ standard requirement
  if(DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR
      "VillageSQL Extension SDK requires C++17 or later. "
      "CMAKE_CXX_STANDARD is set to ${CMAKE_CXX_STANDARD}."
    )
  endif()

  # Chain to the full CMake config for VEF_CREATE_VEB, etc.
  find_package(VillageSQLExtensionFramework REQUIRED
    PATHS "${VillageSQL_PREFIX}"
    NO_DEFAULT_PATH
  )

  # Re-export the include dir from the framework config for convenience
  if(NOT VillageSQLExtensionFramework_INCLUDE_DIR)
    set(VillageSQLExtensionFramework_INCLUDE_DIR "${VillageSQL_INCLUDE_DIR}")
  endif()
endif()

mark_as_advanced(VILLAGESQL_CONFIG_EXECUTABLE)
