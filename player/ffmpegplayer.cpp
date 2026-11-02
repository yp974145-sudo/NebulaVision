#include "ffmpegplayer.h"
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QUrl>
#include <QMutexLocker>

DecodeTask::DecodeTask(const QString &source, const QString &executable, QObject *parent)
    : QThread(parent), stopping(false), paused(false)
{
    this->source = source;
    this->executable = executable;
}
void DecodeTask::requestStop() { stopping.store(true); }
void DecodeTask::setPaused(bool value) { paused.store(value); }
bool DecodeTask::takeFrame(QImage *frame)
{
    QMutexLocker lock(&frameMutex);
    if (!hasFrame) return false;
    *frame = latestFrame;
    latestFrame = QImage();
    hasFrame = false;
    return true;
}
int DecodeTask::queuedFrames() { QMutexLocker lock(&frameMutex); return hasFrame ? 1 : 0; }
int DecodeTask::droppedFrames() { QMutexLocker lock(&frameMutex); return dropped; }
void DecodeTask::run()
{
    QProcess process; // 在工作线程创建和销毁，不跨线程调用QProcess。
    QStringList arguments;
    arguments << "-hide_banner" << "-loglevel" << "error" << "-nostdin";
    bool network = source.startsWith("rtsp://") || source.startsWith("rtsps://");
    if (network) {
        arguments << "-rtsp_transport" << "tcp" << "-timeout" << "3000000";
    } else {
        arguments << "-re";
    }
    arguments << "-i" << source << "-an" << "-sn";
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
            {
                // 容量1的最新帧缓存：界面来不及显示就覆盖旧帧，不能无限排队。
                QMutexLocker lock(&frameMutex);
                if (hasFrame) ++dropped;
                latestFrame = frame.copy();
                hasFrame = true;
            }
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
    surface->setMinimumSize(160,90);
    // 不让上一帧图片的尺寸成为布局的最小尺寸。
    surface->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);
    surface->setText("FFmpeg视频画面");
    surface->setStyleSheet("background:#111;color:white;");
    renderTimer.setInterval(33);
    connect(&renderTimer,&QTimer::timeout,this,[this]() {
        if (!enabled || !task || userPaused) return;
        QImage frame;
        if (!task->takeFrame(&frame)) return;
        retryCount = 0;
        surface->setPixmap(QPixmap::fromImage(frame).scaled(surface->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
        emit statusChanged("正在播放（FFmpeg静音预览）");
        emit frameDisplayed();
    });
    retryTimer.setSingleShot(true);
    connect(&retryTimer,&QTimer::timeout,this,[this]() {
        if (enabled && networkSource) startDecoding();
    });
}
FfmpegPlayer::~FfmpegPlayer() { stop(); }
QWidget *FfmpegPlayer::videoWidget() { return surface; }
void FfmpegPlayer::open(const QString &source)
{
    stop();
    currentSource.clear();
    QUrl url(source);
    networkSource = url.scheme()=="rtsp" || url.scheme()=="rtsps";
    if (networkSource) {
        if (!url.isValid() || url.host().isEmpty()) {
            emit playbackError("RTSP地址格式不正确。");
            return;
        }
        currentSource = source;
    } else {
        if (!QFileInfo(source).isFile()) {
            emit playbackError("视频文件不存在，或不支持该协议。");
            return;
        }
        currentSource = QFileInfo(source).absoluteFilePath();
    }
    if (!QFileInfo(ffmpegExecutable()).isFile()) {
        emit playbackError("找不到FFmpeg工具，请配置NEBULAVISION_FFMPEG。");
        return;
    }
    enabled = true;
    userPaused = false;
    renderTimer.start();
    retryCount = 0;
    startDecoding();
}
void FfmpegPlayer::startDecoding()
{
    clearTask();
    ++generation;
    int session = generation;
    task = new DecodeTask(currentSource,ffmpegExecutable(),this);
    connect(task,&DecodeTask::failed,this,[this,session](const QString &message) {
        if (session == generation && enabled) emit playbackError(message);
    },Qt::QueuedConnection);
    connect(task,&QThread::finished,this,[this,session]() {
        if (session != generation || !enabled) return;
        if (networkSource) {
            // 1、2、4、8秒退避，之后保持8秒；停止时取消定时器。
            int delay = 1000 * (1 << qMin(retryCount,3));
            ++retryCount;
            retryTimer.start(delay);
            emit statusChanged(QString("断流，%1秒后重连").arg(delay/1000));
            emit retryScheduled(delay);
        } else {
            emit statusChanged("本地视频播放结束");
        }
    });
    task->start();
    emit statusChanged("FFmpeg正在连接（静音预览）");
}
void FfmpegPlayer::pause()
{
    if (task && task->isRunning()) {
        userPaused = true;
        task->setPaused(true);
        emit statusChanged("已暂停");
    }
}
void FfmpegPlayer::resume()
{
    if (task && task->isRunning()) {
        userPaused = false;
        task->setPaused(false);
        emit statusChanged("正在播放");
    } else if (!currentSource.isEmpty()) {
        // 停止和播放结束后任务已退出，重新创建任务从头播放。
        // 用副本：open会清理成员currentSource，不能把同一个成员引用传进去。
        QString source = currentSource;
        open(source);
    } else {
        emit statusChanged("请先打开视频或RTSP地址");
    }
}
void FfmpegPlayer::clearTask()
{
    if (task) {
        disconnect(task,nullptr,this,nullptr);
        task->requestStop();
        task->wait();
        delete task;
        task = nullptr;
    }
}
void FfmpegPlayer::stop()
{
    enabled = false;
    ++generation; // 已排队的旧帧即使送达，也不能更新新会话。
    retryTimer.stop();
    renderTimer.stop();
    clearTask();
    surface->clear();
    surface->setText("已停止");
    emit statusChanged("已停止");
}

int FfmpegPlayer::queuedFrames() const { return task ? task->queuedFrames() : 0; }
int FfmpegPlayer::droppedFrames() const { return task ? task->droppedFrames() : 0; }
