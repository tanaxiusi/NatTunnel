HEADERS += ./Function/ClientManager.h \
    ./Function/UserContainer.h \
    ./Util/Other.h \
    ./Util/QStringMap.h \
    ../Shared/MessageConverter.h \
    ../Shared/crc32/crc32.h \
    ../Shared/aes/aes.h
SOURCES += ./Function/ClientManager.cpp \
    ./Function/UserContainer.cpp \
    ./Function/main.cpp \
    ./Util/Other.cpp \
    ./Util/QStringMap.cpp \
    ../Shared/MessageConverter.cpp \
    ../Shared/crc32/crc32.cpp \
    ../Shared/aes/aes.c
	
win32{
	RC_FILE = Resources/Server.rc
}
