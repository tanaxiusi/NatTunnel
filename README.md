#NatTunnel是什么？可以用来干什么？
    NatTunnel·Server+Client是一套独立的公网辅助内网穿透软件。
    把NatTunnel·Server放置在一台公网服务器上，各个NatTunnel·Client可以互相建立p2p连接，连接成功后，可以添加Tcp端口转发。
    
#编译
    需要Qt4或5，至少需要vs2008环境。Linux下理论上也可以用，不过有些功能支持得不如Windows好。
    
#配置Server环境
    把编译出的Server.exe和Client.exe放在单独目录下，Qt相关dll也放在同一目录，需要哪些库下面有说明。
    创建一个文件，命名为NatTunnelServer.ini，输入以下内容：

    [Other]
    GlobalKey=YourKey       传输密钥，自行设置，所有连接到这个Server的Client都必须设置相同的密钥

    [Port]                  Server需要1个Tcp端口用于维持长连接，2个Udp端口用于功能支持
    Tcp=9991
    Udp1=9992
    Udp2=9993

    [Binary]
    Windows=Client.exe      把Client.exe放在目录下，这里填上文件名，用于版本检查，如果Client版本不对，Server会把正确的exe文件传过去，然后自动更新

#配置Client环境
    把Client.exe和Qt相关dll放在同一目录即可。
    
#依赖库
    Server：QtCore、QtNetwork、QtSql，还需要sqldrivers/qsqlite.dll。
    Client：QtCore、QtGui、QtWidgets(Qt5需要)、QtNetwork、QtXml。可能需要bearer/qgenericbearer4.dll和bearerqnativewifibearer4.dll，这两个不是很确定。
    
###运行Client，第一次运行会要求输入Server信息：
![image](Images/Client-01.png)
###填入Server端对应信息后，输入一个自己的用户名：
![image](Images/Client-03.png)
###登录成功后如图：
![image](Images/Client-02.png)

    左下角的下拉框会显示在线用户，点左边蓝色按钮刷新，选中一个用户，点击“开始连接”，输入对方本地密码，就可以连接了。
    连接成功后，右边表格会多出一行，点“管理转发”可以添加或查看Tcp转发，目前可以查看转入/转出，但是只能添加转出。

#关于登录验证
    首次登录会随机产生一个本地密码，被别人连接时需要验证本地密码，本地密码可以修改。
    Client首次运行生成的配置文件NatTunnelClient.ini中包含了随机标识符(RandomIdentifierSuffix)，也在第一次运行时随机产生。
    Client用随机标识符+网卡MAC地址作为机器ID，也就是说，如果更换网卡，或者删除配置文件，都会被认成另一台机器。
    如果有需要，同一个机器ID可以随意修改用户名，把配置文件的“UserName=”删掉，重新运行，就会让你重新输入，但是不能用别人已经占用还没弃用的用户名。
    所以，如果不小心变更了机器ID，那么原来的ID占用的用户名将不能重新使用，当然也可以用Sqlite编辑器手动修改Server目录下的User.db。
    
#没有公网服务器，但是有公网端口，可以用吗？
    可以。
    必须暴露1个Tcp端口和2个Udp端口，其中Udp内外端口号要一致。
    
#只能在Windows系统使用吗？
    由于用的是Qt，所以理论上也可以在其他系统上用，但是我没有测试过。
    我对其他系统不熟悉，目前只能在Windows下获取网关ip地址和网关mac地址，用来判断两个Client是否在同一内网。
    所以，如果在其他系统下使用，同一内网的机器可能无法直连。

#为什么有时候会提示“Nat类型是xxx，无法连接”？
    NAT分为4种：
    1.Full Cone NAT
    2.Restricted Cone NAT
    3.Port Restricted Cone NAT
    4.Symmetric NAT
    一般情况下3+4和4+4是无法直连的。

#既然不能直连，为什么不用服务器转发，或者是端口猜测？
    服务器转发：逻辑写起来太麻烦了，还占用服务器大量带宽。
    至于端口猜测。
    据我所知，现在大部分用Symmetric NAT的地方，大概是内网托管机房、手机3G/4G上网、公共wifi等环境，而且实测NAT出去的端口号都是随机的，这种情况下基本不可能猜到端口，所以猜端口号基本没有意义。

#Upnp在什么情况下有用？
    在上面说的3+4或4+4的情况下，某一边有upnp，并且路由器外面是公网IP，就可以突破原环境限制，进行直连。
    不过这种情况比较罕见，所以upnp到底能不能用，我也没有测试过。
    
#直连成功了，我想通过Windows远程桌面连接对方，但是等了一会掉线了，为什么？
    这是Windows系统的限制。
    使用远程桌面，验证用户名和密码后，有一个阶段，所有运行在该用户窗口站(WindowStation)中的程序会被暂停。
    如果本程序也恰好运行在其中，就会陷入死锁状态：远程桌面Server在等待数据，而NatTunnel·Client被暂停，无法传输数据，最终引发超时断线。
    可以在对方机器上用另外一个用户运行本程序；或者用计划任务运行，勾上“不管用户是否登录都要运行”选项。
    
#直连用的是udp吗？具体是什么协议？
    是udp，用的是kcp协议https://github.com/skywind3000/kcp
