idf_component_register(SRCS 
        SerialSpine.cpp 
        CborSerializer.cpp 
        CborDeserializer.cpp 
        ppp_frame.cpp 
        CborDump.cpp 
        Frame.cpp
    INCLUDE_DIRS 
        . 
        ../../main  
        ../../../etl/include
    REQUIRES 
        "limero cbor" )
