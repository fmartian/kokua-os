# -*- cmake -*-
include(Prebuilt)
include(Linking)
include(ZLIBNG)

include_guard()
add_library( ll::libpng INTERFACE IMPORTED )

use_system_binary(libpng)
use_prebuilt_binary(libpng)

find_library(LIBPNG_LIBRARY
    NAMES
    libpng16.lib
    libpng16.a
    PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

target_link_libraries(ll::libpng INTERFACE ${LIBPNG_LIBRARY} ll::zlib-ng)
target_include_directories(ll::libpng SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/libpng16)
if (LINUX)
  #
  # When we have updated static libraries in competition with older
  # shared libraries and we want the former to win, we need to do some
  # extra work.  The *_PRELOAD_ARCHIVES settings are invoked early
  # and will pull in the entire archive to the binary giving it 
  # priority in symbol resolution.  Beware of cmake moving the
  # achive load itself to another place on the link command line.  If
  # that happens, you can try something like -Wl,-lpng16 here to hide
  # the archive.  Also be aware that the linker will not tolerate a
  # second whole-archive load of the archive.  See viewer's
  # CMakeLists.txt for more information.
  #
  set(PNG_PRELOAD_ARCHIVES -Wl,--whole-archive png16 -Wl,--no-whole-archive)
endif ()
