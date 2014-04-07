CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(telephony-daemon C)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "\${prefix}")
SET(LIBDIR "\${prefix}/lib")
SET(INCLUDEDIR "\${prefix}/include")

# Set required packages
INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED glib-2.0 gobject-2.0 tcore dlog libsystemd-daemon)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include/)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wdeclaration-after-statement -Wmissing-declarations -Wredundant-decls -Wcast-align")

ADD_DEFINITIONS("-DFEATURE_TLOG_DEBUG")
ADD_DEFINITIONS("-DTCORE_LOG_TAG=\"DAEMON\"")
ADD_DEFINITIONS("-DDAEMON_VERSION=\"${VERSION}\"")
ADD_DEFINITIONS("-DDEFAULT_PLUGINS_PATH=\"/usr/lib/telephony/plugins/\"")

IF (ENABLE_MONITOR)
	ADD_DEFINITIONS("-DENABLE_MONITOR")
ENDIF (ENABLE_MONITOR)

MESSAGE(${CMAKE_C_FLAGS})
MESSAGE(${pkgs_LDFLAGS})

IF (ENABLE_MONITOR)
SET(SRCS
        src/main.c
        src/monitor.c
)
ELSE (ENABLE_MONITOR)
SET(SRCS
        src/main.c
)
ENDIF (ENABLE_MONITOR)

# binary build
ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} "-ldl")


# install
INSTALL(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION /usr/share/license RENAME telephony-daemon)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/systemd/telephony.service DESTINATION /usr/lib/systemd/system)
