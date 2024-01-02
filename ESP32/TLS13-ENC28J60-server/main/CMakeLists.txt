set(srcs "enc28j60_example_main.c"
         "tls_smp_server_task.c"
   )


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWOLFSSL_USER_SETTINGS")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS
                         "."
                         "./include"
                      )

# set_source_files_properties(esp_eth_phy_enc28j60.c esp_eth_mac_enc28j60.c PROPERTIES HEADER_FILE_ONLY TRUE)
