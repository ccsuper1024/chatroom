# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/chenchao/code/cplusplus/chatroom/third_party/json"
  "/home/chenchao/code/cplusplus/chatroom/_deps/json-build"
  "/home/chenchao/code/cplusplus/chatroom/_deps/json-subbuild/json-populate-prefix"
  "/home/chenchao/code/cplusplus/chatroom/_deps/json-subbuild/json-populate-prefix/tmp"
  "/home/chenchao/code/cplusplus/chatroom/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp"
  "/home/chenchao/code/cplusplus/chatroom/_deps/json-subbuild/json-populate-prefix/src"
  "/home/chenchao/code/cplusplus/chatroom/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/chenchao/code/cplusplus/chatroom/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/chenchao/code/cplusplus/chatroom/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
