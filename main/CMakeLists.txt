idf_component_register(SRCS 
        main.cpp 
        LedBlinker.cpp 
        ../../common/CborWriter.cpp 
        ../../common/CborReader.cpp 
        ../../common/Crc32.cpp
        ../../common/Log.cpp
        ../../common/limero.cpp
    INCLUDE_DIRS 
        ../components/ultrasonic 
        ../components/limero 
        ../components/spine  
        ../components/gps
    REQUIRES 
        "limero spine spi_flash  cbor")

# component_compile_options("-fexceptions")
