# Copyright (c) 2025 VillageSQL Contributors
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.16)
project(vsql_ai_extension)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Extension name
set(EXTENSION_NAME "vsql_ai")

# Find VillageSQL SDK
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(VillageSQL REQUIRED)

message(STATUS "VillageSQL SDK found at: ${VillageSQL_PREFIX}")
message(STATUS "VillageSQL include dir: ${VillageSQL_INCLUDE_DIR}")

# Find OpenSSL (required for HTTPS in cpp-httplib)
find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL include: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")

# Create the AI extension shared library
add_library(ai_ext SHARED
    src/http_client.cc
    src/ai_providers.cc
    src/ai_functions.cc
)

# Include directories
target_include_directories(ai_ext PRIVATE
    ${VillageSQLExtensionFramework_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include  # For cpp-httplib and nlohmann/json
)

# Link OpenSSL
target_link_libraries(ai_ext PRIVATE ${OPENSSL_LIBRARIES})

# Create the VEB package
VEF_CREATE_VEB(
    NAME ${EXTENSION_NAME}
    LIBRARY_TARGET ai_ext
    MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
)

# Installation (optional - if user wants to install to a specific location)
if(DEFINED VillageSQL_VEB_INSTALL_DIR)
    install(FILES ${VEB_OUTPUT} DESTINATION ${VillageSQL_VEB_INSTALL_DIR})
    install(CODE "message(STATUS \"${EXTENSION_NAME} extension installed to: ${VillageSQL_VEB_INSTALL_DIR}/${EXTENSION_NAME}.veb\")")
endif()
