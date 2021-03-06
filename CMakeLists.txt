CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(telephony-daemon C)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "\${prefix}")
SET(LIBDIR "\${prefix}/lib")
SET(INCLUDEDIR "\${prefix}/include")

# Set required packages
INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED glib-2.0 gobject-2.0 tcore dlog)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include/)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -Werror -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wdeclaration-after-statement -Wmissing-declarations -Wredundant-decls -Wcast-align")

ADD_DEFINITIONS("-DFEATURE_DLOG_DEBUG")
ADD_DEFINITIONS("-DTCORE_LOG_TAG=\"DAEMON\"")

MESSAGE(${CMAKE_C_FLAGS})
MESSAGE(${pkgs_LDFLAGS})

SET(SRCS
		src/main.c
		src/monitor.c
)


# binary build
ADD_EXECUTABLE(telephony-daemon ${SRCS})
TARGET_LINK_LIBRARIES(telephony-daemon ${pkgs_LDFLAGS} "-ldl")


# install
INSTALL(TARGETS telephony-daemon
		RUNTIME DESTINATION bin)

INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resource/etc/rc.d/init.d/telephony-daemon DESTINATION /etc/rc.d/init.d/
		        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resource/etc/rc.d/rc3.d/S30telephony-daemon DESTINATION /etc/rc.d/rc3.d/
		        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_WRITE WORLD_EXECUTE)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resource/etc/rc.d/rc5.d/S30telephony-daemon DESTINATION /etc/rc.d/rc5.d/
		        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_WRITE WORLD_EXECUTE)
