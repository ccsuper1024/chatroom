# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/chenchao/code/cplusplus/chatroom/third_party/googletest"
  "/home/chenchao/code/cplusplus/chatroom/_deps/googletest-build"
  "/home/chenchao/code/cplusplus/chatroom/_deps/googletest-subbuild/googletest-populate-prefix"
  "/home/chenchao/code/cplusplus/chatroom/_deps/googletest-subbuild/googletest-populate-prefix/tmp"
  "/home/chenchao/code/cplusplus/chatroom/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp"
  "/home/chenchao/code/cplusplus/chatroom/_deps/googletest-subbuild/googletest-populate-prefix/src"
  "/home/chenchao/code/cplusplus/chatroom/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/chenchao/code/cplusplus/chatroom/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/chenchao/code/cplusplus/chatroom/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
