#include "qtvideoplayer.h"
#include <QFileInfo>
#include <QUrl>
QtVideoPlayer::QtVideoPlayer(QWidget *parent) : VideoPlayer(parent)
{
    // 播放器对象由this管理，显示控件由窗口父子关系管理。
    media = new QMediaPlayer(this);
    surface = new QVideoWidget(parent);
    surface->setMinimumSize(320,180);
    media->setVideoOutput(surface);
    // Qt6修改了媒体接口；通过编译条件保留Qt5写法，不在运行时猜版本。
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    audio = new QAudioOutput(this);
    media->setAudioOutput(audio);
    connect(media,&QMediaPlayer::playbackStateChanged,this,[this](QMediaPlayer::PlaybackState state) {
        if(state==QMediaPlayer::PlayingState) emit statusChanged("正在播放");
        if(state==QMediaPlayer::PausedState) emit statusChanged("已暂停");
        if(state==QMediaPlayer::StoppedState) emit statusChanged("已停止");
    });
    connect(media,&QMediaPlayer::errorOccurred,this,[this](QMediaPlayer::Error,const QString &) {
        emit playbackError("Qt媒体后端无法播放，请检查文件与本机解码器。");
    });
#else
    connect(media, &QMediaPlayer::stateChanged, this, [this](QMediaPlayer::State state) {
        if (state == QMediaPlayer::PlayingState) emit statusChanged("正在播放");
        if (state == QMediaPlayer::PausedState) emit statusChanged("已暂停");
        if (state == QMediaPlayer::StoppedState) emit statusChanged("已停止");
    });
    connect(media, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this,
            [this](QMediaPlayer::Error) { emit playbackError("Qt媒体后端无法播放，请检查文件与本机解码器。"); });
#endif
}
QWidget *QtVideoPlayer::videoWidget() { return surface; }
void QtVideoPlayer::open(const QString &source)
{
    stop();
    if (!QFileInfo(source).isFile()) {
        emit playbackError("视频文件不存在。");
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    media->setSource(QUrl::fromLocalFile(QFileInfo(source).absoluteFilePath()));
#else
    media->setMedia(QUrl::fromLocalFile(QFileInfo(source).absoluteFilePath()));
#endif
    media->play();
}
void QtVideoPlayer::pause() { media->pause(); }
void QtVideoPlayer::resume() { media->play(); }
void QtVideoPlayer::stop() { media->stop(); }
