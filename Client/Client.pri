HEADERS += ./Function/Peer.h \
    ./Function/Client.h \
    ./Function/KcpManager.h \
    ./Function/TransferManager.h \
    ./Function/TcpTransfer.h \
    ./Function/QUpnpPortMapper.h \
    ./Resources/resource.h \
    ./UI/MainDlg.h \
    ./UI/GuideDlg.h \
	./UI/MultiLineInputDialog.h \
    ../Shared/MessageConverter.h \
    ../Shared/kcp/ikcp.h \
    ../Shared/crc32/crc32.h \
    ../Shared/aes/aes.h \
    ./Util/Other.h
SOURCES += ./Function/Client.cpp \
    ./Function/KcpManager.cpp \
    ./Function/main.cpp \
    ./Function/Peer.cpp \
    ./Function/QUpnpPortMapper.cpp \
    ./Function/TcpTransfer.cpp \
    ./Function/TransferManager.cpp \
    ./UI/GuideDlg.cpp \
    ./UI/MainDlg.cpp \
	./UI/MultiLineInputDialog.cpp \
    ../Shared/MessageConverter.cpp \
    ../Shared/kcp/ikcp.c \
    ../Shared/crc32/crc32.cpp \
    ../Shared/aes/aes.c \
    ./Util/Other.cpp
FORMS += ./FormFiles/GuideDlg.ui \
    ./FormFiles/MainDlg.ui \
	./FormFiles/MultiLineInputDialog.ui
RESOURCES += Resources/MainDlg.qrc

win32{
	RC_FILE = Resources/Client.rc
}
