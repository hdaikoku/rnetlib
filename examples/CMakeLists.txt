project(bandwidth_client)
project(bandwidth_server)
project(bi_bandwidth_client)
project(bi_bandwidth_server)
project(echo_client)
project(echo_server)
project(sg_bandwidth_client)
project(sg_bandwidth_server)
project(sg_echo_client)
project(sg_echo_server)

set(SOURCE_COMMON
        "${RNETLIB_INCLUDE_DIR}/channel.h"
        "${RNETLIB_INCLUDE_DIR}/event_handler.h"
        "${RNETLIB_INCLUDE_DIR}/event_loop.h"
        "${RNETLIB_INCLUDE_DIR}/local_memory_region.h"
        "${RNETLIB_INCLUDE_DIR}/remote_memory_region.h"
        "${RNETLIB_INCLUDE_DIR}/rnetlib.h"
        "${RNETLIB_INCLUDE_DIR}/socket/socket_channel.h"
        "${RNETLIB_INCLUDE_DIR}/socket/socket_common.h"
        "${RNETLIB_INCLUDE_DIR}/socket/socket_event_loop.h"
        "${RNETLIB_INCLUDE_DIR}/socket/socket_local_memory_region.h")

set(SOURCE_CLIENT
        "${RNETLIB_INCLUDE_DIR}/client.h"
        "${RNETLIB_INCLUDE_DIR}/socket/socket_client.h")

set(SOURCE_SERVER
        "${RNETLIB_INCLUDE_DIR}/server.h"
        "${RNETLIB_INCLUDE_DIR}/socket/socket_server.h")

if (RNETLIB_USE_RDMA)
    set(SOURCE_COMMON ${SOURCE_COMMON}
            "${RNETLIB_INCLUDE_DIR}/verbs/verbs_channel.h"
            "${RNETLIB_INCLUDE_DIR}/verbs/verbs_common.h"
            "${RNETLIB_INCLUDE_DIR}/verbs/verbs_event_loop.h"
            "${RNETLIB_INCLUDE_DIR}/verbs/verbs_local_memory_region.h")

    set(SOURCE_CLIENT ${SOURCE_CLIENT}
            "${RNETLIB_INCLUDE_DIR}/verbs/verbs_client.h")

    set(SOURCE_SERVER ${SOURCE_SERVER}
            "${RNETLIB_INCLUDE_DIR}/verbs/verbs_server.h")
endif (RNETLIB_USE_RDMA)

add_executable(bandwidth_client bandwidth_client.cc ${SOURCE_COMMON} ${SOURCE_CLIENT})
add_executable(bandwidth_server bandwidth_server.cc ${SOURCE_COMMON} ${SOURCE_SERVER})
add_executable(bi_bandwidth_client bi_bandwidth_client.cc ${SOURCE_COMMON} ${SOURCE_CLIENT})
add_executable(bi_bandwidth_server bi_bandwidth_server.cc ${SOURCE_COMMON} ${SOURCE_SERVER})
add_executable(echo_client echo_client.cc ${SOURCE_COMMON} ${SOURCE_CLIENT})
add_executable(echo_server echo_server.cc ${SOURCE_COMMON} ${SOURCE_SERVER})
add_executable(sg_bandwidth_client sg_bandwidth_client.cc ${SOURCE_COMMON} ${SOURCE_CLIENT})
add_executable(sg_bandwidth_server sg_bandwidth_server.cc ${SOURCE_COMMON} ${SOURCE_SERVER})
add_executable(sg_echo_client sg_echo_client.cc ${SOURCE_COMMON} ${SOURCE_CLIENT})
add_executable(sg_echo_server sg_echo_server.cc ${SOURCE_COMMON} ${SOURCE_SERVER})

if (RNETLIB_USE_RDMA)
    add_definitions(-DRNETLIB_USE_RDMA)
    target_link_libraries(bandwidth_client rdmacm ibverbs)
    target_link_libraries(bandwidth_server rdmacm ibverbs)
    target_link_libraries(bi_bandwidth_client rdmacm ibverbs)
    target_link_libraries(bi_bandwidth_server rdmacm ibverbs)
    target_link_libraries(echo_client rdmacm ibverbs)
    target_link_libraries(echo_server rdmacm ibverbs)
    target_link_libraries(sg_bandwidth_client rdmacm ibverbs)
    target_link_libraries(sg_bandwidth_server rdmacm ibverbs)
    target_link_libraries(sg_echo_client rdmacm ibverbs)
    target_link_libraries(sg_echo_server rdmacm ibverbs)
endif (RNETLIB_USE_RDMA)
