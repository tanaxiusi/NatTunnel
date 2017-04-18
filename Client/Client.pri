HEADERS += ../Shared/MessageConverter.h \
    ../Shared/aes/aes.h \
    ../Shared/crc32/crc32.h \
    ../Shared/kcp/ikcp.h \
    ./Service/ServiceMain.h \
    ./Service/Function/Client.h \
    ./Service/Function/KcpManager.h \
    ./Service/Function/QUpnpPortMapper.h \
    ./Service/Function/TcpTransfer.h \
    ./Service/Function/TransferManager.h \
    ./Gui/GuiMain.h \
    ./Gui/UI/AddTransferDlg.h \
    ./Gui/UI/GuideDlg.h \
    ./Gui/UI/MainDlg.h \
    ./Gui/UI/TransferManageDlg.h \
    ./Util/Other.h \
    ./Util/Peer.h \
    ./Gui/BridgeForGui.h \
    ./Service/BridgeForService.h
SOURCES += ./main.cpp \
    ../Shared/MessageConverter.cpp \
    ../Shared/aes/aes.c \
    ../Shared/crc32/crc32.cpp \
    ../Shared/kcp/ikcp.c \
    ./Service/ServiceMain.cpp \
    ./Service/Function/Client.cpp \
    ./Service/Function/KcpManager.cpp \
    ./Service/Function/QUpnpPortMapper.cpp \
    ./Service/Function/TcpTransfer.cpp \
    ./Service/Function/TransferManager.cpp \
    ./Gui/GuiMain.cpp \
    ./Gui/UI/AddTransferDlg.cpp \
    ./Gui/UI/GuideDlg.cpp \
    ./Gui/UI/MainDlg.cpp \
    ./Gui/UI/TransferManageDlg.cpp \
    ./Gui/BridgeForGui.cpp \
    ./Service/BridgeForService.cpp \
    ./Util/Other.cpp \
    ./Util/Peer.cpp
FORMS += ./Gui/FormFiles/AddTransferDlg.ui \
    ./Gui/FormFiles/GuideDlg.ui \
    ./Gui/FormFiles/MainDlg.ui \
    ./Gui/FormFiles/TransferManageDlg.ui
RESOURCES += Gui/Resources/MainDlg.qrc
win32{
    RC_FILE = WindowsResources/Client.rc
    LIBS += Advapi32.lib
}