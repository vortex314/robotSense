idf_component_register(SRCS 
        SerialSpineJson.cpp 
        ppp_frame.cpp 
        Frame.cpp
    INCLUDE_DIRS 
        . 
        ../../main  
    REQUIRES 
        "limero cbor" )
