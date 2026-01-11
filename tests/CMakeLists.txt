cmake_minimum_required(VERSION 3.15)

add_executable(test_neuron test_neuron.c)
target_link_libraries(test_neuron PRIVATE engram)
add_test(NAME test_neuron COMMAND test_neuron)

add_executable(test_synapse test_synapse.c)
target_link_libraries(test_synapse PRIVATE engram)
add_test(NAME test_synapse COMMAND test_synapse)

add_executable(test_encoding test_encoding.c)
target_link_libraries(test_encoding PRIVATE engram)
add_test(NAME test_encoding COMMAND test_encoding)

add_executable(test_recall test_recall.c)
target_link_libraries(test_recall PRIVATE engram)
add_test(NAME test_recall COMMAND test_recall)

add_executable(test_consolidation test_consolidation.c)
target_link_libraries(test_consolidation PRIVATE engram)
add_test(NAME test_consolidation COMMAND test_consolidation)

add_executable(test_arousal test_arousal.c)
target_link_libraries(test_arousal PRIVATE engram)
add_test(NAME test_arousal COMMAND test_arousal)

add_executable(test_governor test_governor.c)
target_link_libraries(test_governor PRIVATE engram)
add_test(NAME test_governor COMMAND test_governor)
