# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/chenchao/code/cplusplus/chatroom/third_party/spdlog"
  "/home/chenchao/code/cplusplus/chatroom/_deps/spdlog-build"
  "/home/chenchao/code/cplusplus/chatroom/_deps/spdlog-subbuild/spdlog-populate-prefix"
  "/home/chenchao/code/cplusplus/chatroom/_deps/spdlog-subbuild/spdlog-populate-prefix/tmp"
  "/home/chenchao/code/cplusplus/chatroom/_deps/spdlog-subbuild/spdlog-populate-prefix/src/spdlog-populate-stamp"
  "/home/chenchao/code/cplusplus/chatroom/_deps/spdlog-subbuild/spdlog-populate-prefix/src"
  "/home/chenchao/code/cplusplus/chatroom/_deps/spdlog-subbuild/spdlog-populate-prefix/src/spdlog-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/chenchao/code/cplusplus/chatroom/_deps/spdlog-subbuild/spdlog-populate-prefix/src/spdlog-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/chenchao/code/cplusplus/chatroom/_deps/spdlog-subbuild/spdlog-populate-prefix/src/spdlog-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
