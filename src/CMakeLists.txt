add_executable(tool main.cpp Scanner.cpp ExeFS.cpp RomFS.cpp NCCH.cpp NCSD.cpp)
target_link_libraries(tool fmt)