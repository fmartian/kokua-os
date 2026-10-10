# -*- cmake -*-
# Construct the viewer version number based on the indra/VIEWER_VERSION file

if (NOT DEFINED VIEWER_SHORT_VERSION) # will be true in indra/, false in indra/newview/
    set(VIEWER_VERSION_BASE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/newview/VIEWER_VERSION.txt")
    set(VIEWER_GIT_REPO_PRESENCE "${CMAKE_CURRENT_SOURCE_DIR}/../.git")

    if ( EXISTS $ENV{AUTOBUILD_VARIABLES_FILE} )
            string(REPLACE "/variables" "" VIEWER_VARIABLES_FILE_LOCATION $ENV{AUTOBUILD_VARIABLES_FILE})
            set(VIEWER_VARIABLES_GIT_REPO_PRESENCE "${VIEWER_VARIABLES_FILE_LOCATION}/.git")

        if ( EXISTS ${VIEWER_VARIABLES_GIT_REPO_PRESENCE} )
          find_program(GIT git)
          if (GIT)
            execute_process(COMMAND ${GIT} symbolic-ref -q --short HEAD
                            WORKING_DIRECTORY ${VIEWER_VARIABLES_FILE_LOCATION}
                            RESULT_VARIABLE git_cb_result
                            ERROR_VARIABLE git_cb_error
                            OUTPUT_VARIABLE GIT_CURRENT_VARIABLES_BRANCH
                            OUTPUT_STRIP_TRAILING_WHITESPACE)
            if (NOT ${git_cb_result} EQUAL 0)
              message(SEND_ERROR "Reading git branch for Variables file failed with output:\n${git_cb_error}")
            else (NOT ${git_cb_result} EQUAL 0)
              execute_process(COMMAND ${GIT} rev-list --count ${GIT_CURRENT_VARIABLES_BRANCH}
                            WORKING_DIRECTORY ${VIEWER_VARIABLES_FILE_LOCATION}
                            RESULT_VARIABLE git_rc_result
                            ERROR_VARIABLE git_rc_error
                            OUTPUT_VARIABLE VIEWER_VARIABLES_VERSION_REVISION
                            OUTPUT_STRIP_TRAILING_WHITESPACE)
              if (NOT ${git_rc_result} EQUAL 0)
                message(SEND_ERROR "Getting Variables revision count failed with output:\n${git_rc_error}")
              else (NOT ${git_rc_result} EQUAL 0)
                message(STATUS "Variables revision (from git) ${VIEWER_VARIABLES_VERSION_REVISION} on branch ${GIT_CURRENT_VARIABLES_BRANCH}")
              endif (NOT ${git_rc_result} EQUAL 0)
            endif (NOT ${git_cb_result} EQUAL 0)
         endif (GIT)
            endif ( EXISTS ${VIEWER_VARIABLES_GIT_REPO_PRESENCE} )
        endif ( EXISTS $ENV{AUTOBUILD_VARIABLES_FILE} )

    if ( EXISTS ${VIEWER_VERSION_BASE_FILE} )
        file(STRINGS ${VIEWER_VERSION_BASE_FILE} VIEWER_SHORT_VERSION REGEX "^[0-9]+\\.[0-9]+\\.[0-9]+")
        string(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" VIEWER_VERSION_MAJOR ${VIEWER_SHORT_VERSION})
        string(REGEX REPLACE "^[0-9]+\\.([0-9]+)\\.[0-9]+" "\\1" VIEWER_VERSION_MINOR ${VIEWER_SHORT_VERSION})
        string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" VIEWER_VERSION_PATCH ${VIEWER_SHORT_VERSION})

        if (DEFINED ENV{revision})
           set(VIEWER_VERSION_REVISION $ENV{revision})
           message(STATUS "Revision (from environment): ${VIEWER_VERSION_REVISION}")

        #Autobuild-1.1 forces the build_id to date and time. Kokua uses the local tip hash
        #elseif (DEFINED ENV{AUTOBUILD_BUILD_ID})
        #   set(VIEWER_VERSION_REVISION $ENV{AUTOBUILD_BUILD_ID})
        #   message(STATUS "Revision (from autobuild environment): ${VIEWER_VERSION_REVISION}")

        # if this is a git repo we count the commits to the branch tip
        elseif ( EXISTS ${VIEWER_GIT_REPO_PRESENCE} )
          find_program(GIT git)
          if (GIT)
            execute_process(COMMAND ${GIT} symbolic-ref -q --short HEAD
                            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                            RESULT_VARIABLE git_cb_result
                            ERROR_VARIABLE git_cb_error
                            OUTPUT_VARIABLE GIT_CURRENT_BRANCH
                            OUTPUT_STRIP_TRAILING_WHITESPACE)
            if (NOT ${git_cb_result} EQUAL 0)
              message(SEND_ERROR "Reading git branch failed with output:\n${git_cb_error}")
            else (NOT ${git_cb_result} EQUAL 0)
              execute_process(COMMAND ${GIT} rev-list --count ${GIT_CURRENT_BRANCH}
                            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                            RESULT_VARIABLE git_rc_result
                            ERROR_VARIABLE git_rc_error
                            OUTPUT_VARIABLE VIEWER_VERSION_REVISION
                            OUTPUT_STRIP_TRAILING_WHITESPACE)
              if (NOT ${git_rc_result} EQUAL 0)
                message(SEND_ERROR "Getting revision count failed with output:\n${git_rc_error}")
              else (NOT ${git_rc_result} EQUAL 0)
                message(STATUS "Viewer revision (from git) ${VIEWER_VERSION_REVISION} on branch ${GIT_CURRENT_BRANCH}")
              endif (NOT ${git_rc_result} EQUAL 0)
            endif (NOT ${git_cb_result} EQUAL 0)
          else (GIT)
            if (DEFINED ENV{AUTOBUILD_BUILD_ID})
                 set(VIEWER_VERSION_REVISION $ENV{AUTOBUILD_BUILD_ID})
                 message(STATUS "Viewer revision (from autobuild environment under git (git executable not found)): ${VIEWER_VERSION_REVISION}")
            endif (DEFINED ENV{AUTOBUILD_BUILD_ID})
         endif (GIT)
        endif (DEFINED ENV{revision})
        message(STATUS "Building '${VIEWER_CHANNEL}' Version ${VIEWER_SHORT_VERSION}.${VIEWER_VERSION_REVISION}")
    else ( EXISTS ${VIEWER_VERSION_BASE_FILE} )
        message(SEND_ERROR "Cannot get viewer version from '${VIEWER_VERSION_BASE_FILE}'")
    endif ( EXISTS ${VIEWER_VERSION_BASE_FILE} )

    if ("${VIEWER_VERSION_REVISION}" STREQUAL "")
      message(STATUS "Ultimate fallback, revision was blank or not set: will use 0")
      set(VIEWER_VERSION_REVISION 0)
    endif ("${VIEWER_VERSION_REVISION}" STREQUAL "")
endif (NOT DEFINED VIEWER_SHORT_VERSION)
