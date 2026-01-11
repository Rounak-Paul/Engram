cmake_minimum_required(VERSION 3.15)

add_executable(basic basic.c)
target_link_libraries(basic PRIVATE engram)

add_executable(associative associative.c)
target_link_libraries(associative PRIVATE engram)

add_executable(streaming streaming.c)
target_link_libraries(streaming PRIVATE engram)

add_executable(resource_limited resource_limited.c)
target_link_libraries(resource_limited PRIVATE engram)

add_executable(conversation conversation.c)
target_link_libraries(conversation PRIVATE engram)

add_executable(ingest_knowledge ingest_knowledge.c)
target_link_libraries(ingest_knowledge PRIVATE engram)
