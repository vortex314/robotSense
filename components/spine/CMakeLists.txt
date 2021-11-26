idf_component_register(SRCS 
        Spine.cpp 
        SerialFrame.cpp
        UdpFrame.cpp 
        Wifi.cpp
    INCLUDE_DIRS 
        . 
        ../../main  
    REQUIRES 
        "limero cbor nvs_flash " )
