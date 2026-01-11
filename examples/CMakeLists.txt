add_executable(conversation conversation.c)
target_link_libraries(conversation PRIVATE engram)

add_executable(basic basic.c)
target_link_libraries(basic PRIVATE engram)

add_executable(learn_file learn_file.c)
target_link_libraries(learn_file PRIVATE engram)
