#include "ffmpegplayer.h"
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QElapsedTimer>

DecodeTask::DecodeTask(const QString &source, const QString &executable, QObject *parent)
    : QThread(parent), stopping(false), paused(false)
{
    this->source = source;
    this->executable = executable;
}
void DecodeTask::requestStop() { stopping.store(true); }
void DecodeTask::setPaused(bool value) { paused.store(value); }
void DecodeTask::run()
{
    QProcess process; // 在工作线程创建和销毁，不跨线程调用QProcess。
    QStringList arguments;
    arguments << "-hide_banner" << "-loglevel" << "error" << "-nostdin";
    arguments << "-re" << "-i" << source << "-an" << "-sn";
    arguments << "-vf" << "fps=15,scale=640:360:force_original_aspect_ratio=decrease,pad=640:360:(ow-iw)/2:(oh-ih)/2";
    arguments << "-pix_fmt" << "rgb24" << "-f" << "rawvideo" << "pipe:1";
    // 参数逐项传入，不拼接shell命令，带空格的文件路径也能正确处理。
    process.start(executable, arguments);
    if (!process.waitForStarted(3000)) {
        emit failed("FFmpeg进程启动失败，请检查工具路径。");
        return;
    }
    const int width = 640;
    const int height = 360;
    const int frameBytes = width * height * 3; // RGB24每像素3字节。
    QByteArray buffer;
    QElapsedTimer lastData;
    lastData.start();
    bool receivedFrame = false;
    while (!stopping.load()) {
        if (paused.load()) {
            QThread::msleep(20);
            lastData.restart();
            continue;
        }
        process.waitForReadyRead(50);
        process.readAllStandardError(); // 持续排空stderr，不让错误日志堵住子进程。
        QByteArray chunk = process.read(frameBytes - buffer.size());
        if (!chunk.isEmpty()) {
            buffer.append(chunk);
            lastData.restart();
        }
        if (buffer.size() == frameBytes) {
            // QImage默认引用buffer内存，copy后帧拥有自己的数据，才能跨线程传递。
            QImage frame(reinterpret_cast<const uchar *>(buffer.constData()), width, height, width*3, QImage::Format_RGB888);
            emit frameReady(frame.copy());
            receivedFrame = true;
            buffer.clear();
        }
        if (process.state() == QProcess::NotRunning && process.bytesAvailable() == 0) {
            if (process.exitCode() != 0 || !receivedFrame) emit failed("FFmpeg无法解码该视频。");
            break;
        }
        if (lastData.elapsed() > 5000) {
            emit failed("超过5秒没有视频数据，已停止解码。");
            break;
        }
    }
    // 这里只终止自己的解码子进程，不会结束其他ffmpeg程序。
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(2000);
    }
}

QString FfmpegPlayer::ffmpegExecutable()
{
    QString path = QString::fromUtf8(qgetenv("NEBULAVISION_FFMPEG"));
    if (!path.isEmpty()) return path;
    path = QStandardPaths::findExecutable("ffmpeg");
    if (!path.isEmpty()) return path;
    QDir directory(QCoreApplication::applicationDirPath()+"/../../x/media-tools/imageio-ffmpeg/imageio_ffmpeg/binaries");
    QStringList files = directory.entryList(QStringList()<<"ffmpeg*.exe",QDir::Files);
    if (!files.isEmpty()) return directory.filePath(files.first());
    return QString();
}
FfmpegPlayer::FfmpegPlayer(QWidget *parent) : VideoPlayer(parent)
{
    surface = new QLabel(parent);
    surface->setAlignment(Qt::AlignCenter);
    surface->setMinimumSize(320,180);
    surface->setText("FFmpeg视频画面");
    surface->setStyleSheet("background:#111;color:white;");
}
FfmpegPlayer::~FfmpegPlayer() { stop(); }
QWidget *FfmpegPlayer::videoWidget() { return surface; }
void FfmpegPlayer::open(const QString &source)
{
    stop();
    if (!QFileInfo(source).isFile()) {
        emit playbackError("视频文件不存在。");
        return;
    }
    QString executable = ffmpegExecutable();
    if (!QFileInfo(executable).isFile()) {
        emit playbackError("找不到FFmpeg工具，请配置NEBULAVISION_FFMPEG。");
        return;
    }
    task = new DecodeTask(QFileInfo(source).absoluteFilePath(), executable, this);
    connect(task,&DecodeTask::frameReady,this,[this](const QImage &frame) {
        surface->setPixmap(QPixmap::fromImage(frame).scaled(surface->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
        emit frameDisplayed();
    },Qt::QueuedConnection);
    connect(task,&DecodeTask::failed,this,&VideoPlayer::playbackError,Qt::QueuedConnection);
    connect(task,&QThread::finished,this,[this](){emit statusChanged("播放结束或已停止");});
    task->start();
    emit statusChanged("FFmpeg正在打开视频（预览无音频）");
}
void FfmpegPlayer::pause() { if(task) {task->setPaused(true);emit statusChanged("已暂停");} }
void FfmpegPlayer::resume() { if(task && task->isRunning()) {task->setPaused(false);emit statusChanged("正在播放");} }
void FfmpegPlayer::stop()
{
    if (task) {
        disconnect(task,nullptr,this,nullptr);
        task->requestStop();
        task->wait();
        delete task;
        task = nullptr;
    }
    emit statusChanged("已停止");
}
