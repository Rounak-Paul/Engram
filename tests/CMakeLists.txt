add_executable(test_substrate test_substrate.c)
target_link_libraries(test_substrate PRIVATE engram)
add_test(NAME substrate COMMAND test_substrate)

add_executable(test_synapse test_synapse.c)
target_link_libraries(test_synapse PRIVATE engram)
add_test(NAME synapse COMMAND test_synapse)

add_executable(test_pathway test_pathway.c)
target_link_libraries(test_pathway PRIVATE engram)
add_test(NAME pathway COMMAND test_pathway)

add_executable(test_engram test_engram.c)
target_link_libraries(test_engram PRIVATE engram)
add_test(NAME engram COMMAND test_engram)

add_executable(test_persistence test_persistence.c)
target_link_libraries(test_persistence PRIVATE engram)
add_test(NAME persistence COMMAND test_persistence)
