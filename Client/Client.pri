HEADERS += ./Function/Peer.h \
    ./Function/Client.h \
    ./Function/KcpManager.h \
    ./Function/TransferManager.h \
    ./Function/TcpTransfer.h \
    ./Function/QUpnpPortMapper.h \
    ./Resources/resource.h \
    ./UI/TransferManageDlg.h \
    ./UI/AddTransferDlg.h \
    ./UI/MainDlg.h \
    ./UI/GuideDlg.h \
    ../Shared/MessageConverter.h \
    ../Shared/aes/aes.h \
    ../Shared/crc32/crc32.h \
    ../Shared/kcp/ikcp.h \
    ./Util/Other.h
SOURCES += ./main.cpp \
    ./Function/Client.cpp \
    ./Function/KcpManager.cpp \
    ./Function/Peer.cpp \
    ./Function/QUpnpPortMapper.cpp \
    ./Function/TcpTransfer.cpp \
    ./Function/TransferManager.cpp \
    ./UI/AddTransferDlg.cpp \
    ./UI/GuideDlg.cpp \
    ./UI/MainDlg.cpp \
    ./UI/TransferManageDlg.cpp \
    ../Shared/MessageConverter.cpp \
    ../Shared/aes/aes.c \
    ../Shared/crc32/crc32.cpp \
    ../Shared/kcp/ikcp.c \
    ./Util/Other.cpp
FORMS += ./FormFiles/GuideDlg.ui \
    ./FormFiles/MainDlg.ui \
    ./FormFiles/TransferManageDlg.ui \
    ./FormFiles/AddTransferDlg.ui
RESOURCES += Resources/MainDlg.qrc
