idf_component_register(SRCS 
        main.cpp 
        LedBlinker.cpp 
    INCLUDE_DIRS 
        ../components/ultrasonic 
        ../components/limero 
        ../components/spine  
        ../components/gps
    REQUIRES 
        "limero spine spi_flash  cbor")
