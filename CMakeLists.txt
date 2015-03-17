CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(telephony-daemon C)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "\${prefix}")
SET(LIBDIR "\${prefix}/lib")
SET(INCLUDEDIR "\${prefix}/include")

# Set required packages
INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED glib-2.0 vconf tcore dlog)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include/)

SET(ADDITIONAL_CFLAGS "-Wno-array-bounds -Wno-empty-body -Wno-ignored-qualifiers -Wshadow -Wwrite-strings -Wswitch-default -Wno-unused-but-set-parameter -Wno-unused-but-set-variable")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -Werror -Wextra -Wall -Wdeclaration-after-statement -Wmissing-declarations -Wredundant-decls -Wcast-align -Wcast-qual")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ADDITIONAL_CFLAGS}")

ADD_DEFINITIONS("-DFEATURE_TLOG_DEBUG")
ADD_DEFINITIONS("-DTCORE_LOG_TAG=\"DAEMON\"")
ADD_DEFINITIONS("-DDAEMON_VERSION=\"${VERSION}\"")
ADD_DEFINITIONS("-DDEFAULT_PLUGINS_PATH=\"/usr/lib/telephony/plugins/\"")

IF (TIZEN_DEBUG_ENABLE)
	ADD_DEFINITIONS ("-DTIZEN_DEBUG_ENABLE")
ENDIF (TIZEN_DEBUG_ENABLE)


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
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/packaging/telephony-daemon.service DESTINATION /usr/lib/systemd/system)
