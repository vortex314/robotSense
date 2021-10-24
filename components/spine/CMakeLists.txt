idf_component_register(SRCS 
        SerialSpineJson.cpp 
        ppp_frame.cpp 
        Frame.cpp
    INCLUDE_DIRS 
        . 
        ../../main  
        ../../ArduinoJson/src
        ../../../etl/include
    REQUIRES 
        "limero cbor" )
