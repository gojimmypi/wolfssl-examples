set(srcs "enc28j60_example_main.c"
         "tls_smp_server_task.c"
   )

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ".")

# set_source_files_properties(esp_eth_phy_enc28j60.c esp_eth_mac_enc28j60.c PROPERTIES HEADER_FILE_ONLY TRUE)
