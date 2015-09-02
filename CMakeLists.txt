CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(telephony-daemon C)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "\${prefix}")
SET(LIBDIR ${LIB_INSTALL_DIR})
SET(UNITDIR ${UNIT_INSTALL_DIR})
SET(INCLUDEDIR "\${prefix}/include")

# Set required packages
INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED glib-2.0 vconf tcore dlog)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include/)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -Werror -Wextra -Wall -Wdeclaration-after-statement -Wmissing-declarations -Wredundant-decls -Wcast-align -Wcast-qual -Wno-array-bounds -Wno-empty-body -Wno-ignored-qualifiers -Wshadow -Wwrite-strings -Wswitch-default -Wno-unused-but-set-parameter -Wno-unused-but-set-variable")

##Address-Space Layout Randomization
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")

ADD_DEFINITIONS("-DFEATURE_TLOG_DEBUG")
ADD_DEFINITIONS("-DTCORE_LOG_TAG=\"DAEMON\"")
ADD_DEFINITIONS("-DDAEMON_VERSION=\"${VERSION}\"")
ADD_DEFINITIONS("-DDEFAULT_PLUGINS_PATH=\"${LIBDIR}/telephony/plugins/\"")

IF (TIZEN_DEBUG_ENABLE)
	ADD_DEFINITIONS ("-DTIZEN_DEBUG_ENABLE")
ENDIF (TIZEN_DEBUG_ENABLE)

IF (TIZEN_PROFILE_TV)
	ADD_DEFINITIONS ("-DTIZEN_PROFILE_TV")
ENDIF (TIZEN_PROFILE_TV)

MESSAGE(${CMAKE_C_FLAGS})
MESSAGE(${pkgs_LDFLAGS})

IF (TIZEN_DEBUG_ENABLE)
SET(SRCS
	src/main.c
	src/monitor.c
)
ELSE (TIZEN_DEBUG_ENABLE)
SET(SRCS
	src/main.c
)
ENDIF (TIZEN_DEBUG_ENABLE)

# binary build
ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} "-ldl")

# install
INSTALL(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION /usr/share/license RENAME telephony-daemon)
IF (NOT TIZEN_PROFILE_TV)
	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/packaging/telephony-daemon.service DESTINATION ${UNITDIR})
ELSE (NOT TIZEN_PROFILE_TV)
	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/packaging/telephony-daemon-on-demand.service DESTINATION ${UNITDIR} RENAME telephony-daemon.service)
ENDIF (NOT TIZEN_PROFILE_TV)
