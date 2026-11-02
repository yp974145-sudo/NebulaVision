#include "dbconn.h"
DbConn *DbConn::s_instence;
DbConn *DbConn::getInstence()
{
    if(DbConn::s_instence==nullptr){
        DbConn::s_instence=new DbConn;
    }
    return DbConn ::s_instence;
}

void DbConn::releaseInstence()
{
    if(DbConn::s_instence!=nullptr){
        delete DbConn::s_instence;
        DbConn::s_instence=nullptr;
    }
}

QSqlDatabase DbConn::getDb() const
{
    return db;
}
//实现md5加密
QString DbConn::getMd5(const QString &str)
{
    //1将字符串转换为utf-8编码的字节数组（避免中文乱码）
    QByteArray byteArray=str.toUtf8();
    //2计算MD5的哈希值
    QByteArray md5Arr=QCryptographicHash::hash(byteArray,QCryptographicHash::Md5);
    //3转换为16进制的字符串
    QString md5Str=md5Arr.toHex();
    return md5Str;
}

DbConn::DbConn()
{
    //1注册mysql驱动
     
     db=QSqlDatabase::addDatabase("QMYSQL");
     //2设置数据库连接参数
     db.setHostName("localhost");
     db.setPort(3306);
     db.setUserName("root");
     const QString databasePassword = QString::fromUtf8(qgetenv("NEBULAVISION_LEGACY_DB_PASSWORD"));
     if (databasePassword.isEmpty()) {
         qWarning() << "Missing environment variable: NEBULAVISION_LEGACY_DB_PASSWORD";
         return;
     }
     db.setPassword(databasePassword);
     db.setDatabaseName("smart_monitor");
     //3打开i数据库
     if(!db.open()){
         qDebug()<<"连接数据库失败"<<db.lastError().text();
         return ;
     }
     qDebug()<<"连接成功";
}

DbConn::~DbConn()
{
    qDebug()<<"释放数据库连接";
    QString name=db.connectionName();
    db.close();
    db=QSqlDatabase();
    QSqlDatabase::removeDatabase(name);
}


