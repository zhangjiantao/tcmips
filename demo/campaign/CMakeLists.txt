add_executable(campaign_nim campaign_nim.c)
target_link_libraries(campaign_nim tcmips_core)

add_executable(campaign_rng campaign_rng.c)
target_link_libraries(campaign_rng tcmips_core)

add_executable(campaign_cap campaign_cap.c)
target_link_libraries(campaign_cap tcmips_core)

add_executable(campaign_sort campaign_sort.c)
target_link_libraries(campaign_sort tcmips_core)

add_executable(campaign_hanoi campaign_hanoi.c)
target_link_libraries(campaign_hanoi tcmips_core)
