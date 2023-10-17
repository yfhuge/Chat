aux_source_directory(. SRC_LIST)
aux_source_directory(../ PUB_LIST)

add_executable(ChatClient ${SRC_LIST} ${PUB_LIST})

target_link_libraries(ChatClient protobuf pthread)