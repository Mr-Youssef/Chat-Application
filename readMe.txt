Prerequisites:
    For Windows:
        - Make sure you have a C++ compiler that supports C++11 or later (e.g., MSVC, MinGW-w64).
        - Make sure you have wxWidgets installed and properly configured.
    
    For Linux:
        - Make sure you have a C++ compiler that supports C++11 or later (e.g., GCC, Clang).
        - Install wxWidgets using your package manager (e.g., sudo apt-get install libwxgtk3.0-dev on Ubuntu).

    Note: make sure that these 2 files (wxbase32u_gcc_custom.dll and wxmsw32u_core_gcc_custom.dll) are in 
        the same directory as the executable files, you can find them in 
        "C:\Program Files (x86)\wxWidgets\lib\gcc_x64_dll" directory, so take a copy of them 

To compile server.cpp:
    On Windows:
        g++ -std=c++11 server.cpp -o server.exe -lws2_32 -lwsock32 -lpthread

    On Linux:
        g++ -std=c++11 server.cpp -o server -lpthread

To compile client.cpp:
    On Windows:
        g++ -std=c++11 client.cpp -o client.exe -I"C:\\Program Files (x86)\\wxWidgets\\include" -I"C:\\Program Files (x86)\\wxWidgets\\lib\\gcc_x64_dll\\mswu" -L"C:\\Program Files (x86)\\wxWidgets\\lib\\gcc_x64_dll" -lwxmsw32u_core -lwxbase32u -lws2_32 -lwsock32

    On Linux:
        g++ -std=c++11 client.cpp -o client $(wx-config --cxxflags --libs)
        or
        g++ -std=c++11 client.cpp -o client `wx-config --cxxflags --libs` -lpthread

To run the executables (Windows/ Linux):
    ./server
    ./client
