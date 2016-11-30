HEADERS += ./Function/UserContainer.h \
    ./Function/ClientManager.h \
    ./Util/Other.h \
    ./Util/QStringMap.h \
    ../Shared/MessageConverter.h \
    ../Shared/aes/aes.h \
    ../Shared/crc32/crc32.h
SOURCES += ./Function/ClientManager.cpp \
    ./Function/main.cpp \
    ./Function/UserContainer.cpp \
    ./Util/Other.cpp \
    ./Util/QStringMap.cpp \
    ../Shared/MessageConverter.cpp \
    ../Shared/aes/aes.c \
    ../Shared/crc32/crc32.cpp
win32:RC_FILE = Resources/Server.rc