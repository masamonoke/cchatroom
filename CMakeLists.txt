cmake_minimum_required(VERSION 3.23.2)

set(NAME cchatroom)
project(${NAME})

include_directories(deps/networking/include)
include_directories(common)
include_directories(deps/c-logger)

add_subdirectory(deps/networking)
add_subdirectory(deps/c-logger)

add_executable(cchatroom-client
	client/client.c
)

add_executable(cchatroom-server
	server/server.c
)

add_library(cchatroom-common
	common/control.c
)

target_link_libraries(cchatroom-common c-logger)
target_link_libraries(cchatroom-client networking cchatroom-common)
target_link_libraries(cchatroom-server networking cchatroom-common)

set(CCHATROOM_TARGETS
	cchatroom-client
	cchatroom-server
	cchatroom-common
)

foreach(target IN LISTS CCHATROOM_TARGETS)
	if (NOT MSVC)
		target_compile_options(${target} PRIVATE -Waddress -Wall -Warray-bounds -Wbool-operation -Wchar-subscripts -Wcomment
			-Wmisleading-indentation -Wparentheses -Wextra -pedantic -Wstrict-prototypes -Wshadow -Wconversion -Wvla -Wdouble-promotion -Wmissing-noreturn
			-Wmissing-format-attribute -Wmissing-prototypes -Wunused-result)
	endif()

	if (${CMAKE_BUILD_TYPE} MATCHES Debug)
		if (NOT WIN32 AND NOT APPLE)
			target_link_options(${target} PRIVATE -fsanitize=address,undefined,null,bounds,leak)
		endif()
		if (APPLE)
			target_link_options(${target} PRIVATE -fsanitize=address,undefined,null,bounds)
		endif()
	endif()
endforeach(target IN LISTS TEST_TARGETS)
