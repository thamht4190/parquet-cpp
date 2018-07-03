@rem Licensed to the Apache Software Foundation (ASF) under one
@rem or more contributor license agreements.  See the NOTICE file
@rem distributed with this work for additional information
@rem regarding copyright ownership.  The ASF licenses this file
@rem to you under the Apache License, Version 2.0 (the
@rem "License"); you may not use this file except in compliance
@rem with the License.  You may obtain a copy of the License at
@rem
@rem   http://www.apache.org/licenses/LICENSE-2.0
@rem
@rem Unless required by applicable law or agreed to in writing,
@rem software distributed under the License is distributed on an
@rem "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
@rem KIND, either express or implied.  See the License for the
@rem specific language governing permissions and limitations
@rem under the License.

@echo on

mkdir build
cd build

@rem --------- Cortex configuration ------------- 
set CONFIGURATION=Debug
set GENERATOR=Visual Studio 15 2017 Win64
set BOOST_ROOT=C:/local/boost_1_63_0
set OPENSSL_ROOT_DIR=E:/code/emotiv-cortex-v2/src/ThirdParty/OpenSSL/win64_vs2017
set BUILD_DIR=%cd%
set SNAPPY_STATIC_LIB=%BUILD_DIR%/arrow_ep-prefix/src/arrow_ep-build/snappy_ep/src/snappy_ep-install/lib/snappy_static.lib
set BROTLI_STATIC_LIB_ENC=%BUILD_DIR%/arrow_ep-prefix/src/arrow_ep-build/brotli_ep/src/brotli_ep-install/bin/brotlienc.lib
set BROTLI_STATIC_LIB_DEC=%BUILD_DIR%/arrow_ep-prefix/src/arrow_ep-build/brotli_ep/src/brotli_ep-install/bin/brotlidec.lib
set BROTLI_STATIC_LIB_COMMON=%BUILD_DIR%/arrow_ep-prefix/src/arrow_ep-build/brotli_ep/src/brotli_ep-install/bin/brotlicommon.lib
if "%CONFIGURATION%" == "Debug" (
	set ZLIB_STATIC_LIB=%BUILD_DIR%/arrow_ep-prefix/src/arrow_ep-build/zlib_ep/src/zlib_ep-install/lib/zlibstaticd.lib
	set LZ4_STATIC_LIB=%BUILD_DIR%/arrow_ep-prefix/src/arrow_ep-build/lz4_ep-prefix/src/lz4_ep/visual/VS2010/bin/x64_Debug/liblz4_static.lib
	set ZSTD_STATIC_LIB=%BUILD_DIR%/arrow_ep-prefix/src/arrow_ep-build/zstd_ep-prefix/src/zstd_ep/build/VS2010/bin/x64_DEBUG/libzstd_static.lib
)
if "%CONFIGURATION%" == "Release" (
	set ZLIB_STATIC_LIB=%BUILD_DIR%/arrow_ep-prefix/src/arrow_ep-build/zlib_ep/src/zlib_ep-install/lib/zlibstatic.lib
	set LZ4_STATIC_LIB=%BUILD_DIR%/arrow_ep-prefix/src/arrow_ep-build/lz4_ep-prefix/src/lz4_ep/visual/VS2010/bin/x64_Release/liblz4_static.lib
	set ZSTD_STATIC_LIB=%BUILD_DIR%/arrow_ep-prefix/src/arrow_ep-build/zstd_ep-prefix/src/zstd_ep/build/VS2010/bin/x64_RELEASE/libzstd_static.lib
)
set THRIFT_STATIC_LIB=%BUILD_DIR%/thrift_ep/src/thrift_ep-install/lib/thriftmdd.lib

@rem --------- End of Cortex configuration ------------- 

SET PARQUET_TEST_DATA=%APPVEYOR_BUILD_FOLDER%\data
set PARQUET_CXXFLAGS=/MP

set PARQUET_USE_STATIC_CRT_OPTION=OFF
if "%USE_STATIC_CRT%" == "ON" (
    set PARQUET_USE_STATIC_CRT_OPTION=ON
)

if NOT "%CONFIGURATION%" == "Debug" (
  set PARQUET_CXXFLAGS="%PARQUET_CXXFLAGS% /WX"
)

if NOT "%CONFIGURATION%" == "Toolchain" (
  cmake -G "%GENERATOR%" ^
        -DCMAKE_BUILD_TYPE=%CONFIGURATION% ^
        -DPARQUET_BOOST_USE_SHARED=OFF ^
        -DPARQUET_CXXFLAGS=%PARQUET_CXXFLAGS% ^
		-DPARQUET_BUILD_TESTS=OFF ^
		-DPARQUET_BUILD_BENCHMARKS=OFF ^
		-DPARQUET_ARROW_LINKAGE=static ^
        -DPARQUET_USE_STATIC_CRT=%PARQUET_USE_STATIC_CRT_OPTION% ^
		-DBOOST_ROOT=%BOOST_ROOT% ^
		-DOPENSSL_ROOT_DIR=%OPENSSL_ROOT_DIR% ^
        .. || exit /B

  cmake --build . --config %CONFIGURATION% || exit /B
)

cd ..

