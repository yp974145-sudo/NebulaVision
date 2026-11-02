#include "redisclient.h"
#include <QTcpSocket>
#include <QElapsedTimer>
#include <functional>
QVariant RedisClient::command(int port,const QStringList &arguments,bool *ok) {
    *ok=false;QTcpSocket socket;socket.connectToHost("127.0.0.1",quint16(port));if(!socket.waitForConnected(300))return {};
    QByteArray request="*"+QByteArray::number(arguments.size())+"\r\n";
    for(const QString &argument:arguments){QByteArray value=argument.toUtf8();request+='$'+QByteArray::number(value.size())+"\r\n"+value+"\r\n";}
    socket.write(request);if(socket.bytesToWrite()>0 && !socket.waitForBytesWritten(300))return {};
    QElapsedTimer time;time.start();
    auto need=[&](qint64 count){while(socket.bytesAvailable()<count){if(time.elapsed()>500 || !socket.waitForReadyRead(100))return false;}return true;};
    auto line=[&]()->QByteArray {while(!socket.canReadLine()){if(time.elapsed()>500 || !socket.waitForReadyRead(100))return {};}return socket.readLine().trimmed();};
    if(!need(1))return {};char kind=socket.read(1).at(0);QByteArray header=line();
    if(kind=='+'){*ok=true;return QString::fromUtf8(header);}
    if(kind==':'){bool valid=false;qint64 n=header.toLongLong(&valid);*ok=valid;return n;}
    if(kind=='$'){
        bool valid=false;int length=header.toInt(&valid);if(!valid || length>1024*1024 || length< -1)return {};
        if(length==-1){*ok=true;return {};}
        if(!need(length+2))return {};QByteArray value=socket.read(length);if(socket.read(2)!="\r\n")return {};*ok=true;return QString::fromUtf8(value);
    }
    return {}; // Redis错误或不支持的回复类型，调用者显式降级。
}
