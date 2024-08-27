from subprocess import check_call
import os
import sys
import platform
import multiprocessing


EM_ROOT = os.getenv('EMSDK') +'/upstream/emscripten'
EM_TOOLCHAIN_FILE = EM_ROOT + '/cmake/Modules/Platform/Emscripten.cmake'

if platform.system() == 'Windows':
    MAKE_PROGRAM= 'mingw32-make'
    CMAKE_GENERATOR= 'MinGW Makefiles'
elif platform.system() == 'Linux':
    MAKE_PROGRAM= 'make'
    CMAKE_GENERATOR= 'Unix Makefiles'

cmake_html5_args = ['-DCMAKE_TOOLCHAIN_FILE=' + EM_TOOLCHAIN_FILE , '-DCMAKE_MAKE_PROGRAM=' +MAKE_PROGRAM, '-G' , CMAKE_GENERATOR]
        
cmake_arguments = ['cmake', '-H../', '-B../build_wasm', '-DCMAKE_BUILD_TYPE=Debug']
cmake_arguments.extend(cmake_html5_args)

check_call(cmake_arguments)
numCPUs = multiprocessing.cpu_count()
check_call( ['cmake', '--build', '../build_wasm', '--parallel', str(numCPUs)])