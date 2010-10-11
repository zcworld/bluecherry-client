#include "EventVideoPlayer.h"
#include "EventVideoDownload.h"
#include "video/VideoSurface.h"
#include "core/BluecherryApp.h"
#include "ui/MainWindow.h"
#include <QBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QApplication>
#include <QThread>
#include <QFrame>
#include <QFileDialog>
#include <QShortcut>
#include <QMenu>
#include <QDebug>

EventVideoPlayer::EventVideoPlayer(QWidget *parent)
    : QWidget(parent)
{
    connect(&backend, SIGNAL(stateChanged(int,int)),
            SLOT(stateChanged(int)));
    connect(&backend, SIGNAL(durationChanged(qint64)), SLOT(durationChanged(qint64)));
    connect(&backend, SIGNAL(endOfStream()), SLOT(durationChanged()));

    connect(&m_posTimer, SIGNAL(timeout()), SLOT(updatePosition()));

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);

    m_videoWidget = new VideoContainer(backend.createSurface());
    m_videoWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_videoWidget, SIGNAL(customContextMenuRequested(QPoint)), SLOT(videoContextMenu(QPoint)));
    layout->addWidget(m_videoWidget, 1);

    QBoxLayout *sliderLayout = new QHBoxLayout;
    layout->addLayout(sliderLayout);

    m_seekSlider = new QSlider(Qt::Horizontal);
    connect(m_seekSlider, SIGNAL(valueChanged(int)), SLOT(seek(int)));
    m_seekSlider->setEnabled(false);
    sliderLayout->addWidget(m_seekSlider);

    m_posText = new QLabel;
    sliderLayout->addWidget(m_posText);

    QBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(3);
    layout->addLayout(btnLayout);

    m_playBtn = new QToolButton;
    m_playBtn->setText(QString(QChar(0x25BA)));
    btnLayout->addWidget(m_playBtn);
    connect(m_playBtn, SIGNAL(clicked()), SLOT(playPause()));

    btnLayout->addSpacing(9);

    QToolButton *restartBtn = new QToolButton;
    restartBtn->setText(QString::fromUtf8("|\xe2\x97\x84"));
    btnLayout->addWidget(restartBtn);
    connect(restartBtn, SIGNAL(clicked()), SLOT(restart()));

    btnLayout->addStretch();

    QPushButton *saveBtn = new QPushButton(tr("Save Video"));
    btnLayout->addWidget(saveBtn);
    connect(saveBtn, SIGNAL(clicked()), SLOT(saveVideo()));

    QShortcut *sc = new QShortcut(QKeySequence(Qt::Key_Space), m_videoWidget);
    connect(sc, SIGNAL(activated()), SLOT(playPause()));

    sc = new QShortcut(QKeySequence(Qt::Key_F), m_videoWidget);
    connect(sc, SIGNAL(activated()), m_videoWidget, SLOT(toggleFullScreen()));

    sc = new QShortcut(QKeySequence(Qt::Key_R), m_videoWidget);
    connect(sc, SIGNAL(activated()), SLOT(restart()));

    sc = new QShortcut(QKeySequence::Save, m_videoWidget);
    connect(sc, SIGNAL(activated()), SLOT(saveVideo()));
}

void EventVideoPlayer::setVideo(const QUrl &url)
{
    backend.start(url);
}

void EventVideoPlayer::clearVideo()
{
    backend.clear();
}

void EventVideoPlayer::playPause()
{
    if (backend.state() == VideoPlayerBackend::Playing)
    {
        backend.pause();
    }
    else
    {
        if (backend.atEnd())
            backend.restart();
        backend.play();
    }
}

void EventVideoPlayer::restart()
{
    backend.restart();
    backend.play();
}

void EventVideoPlayer::seek(int position)
{
    qDebug() << "Backend seekable?" << backend.isSeekable();
    backend.seek(qint64(position) * 1000000);
}

void EventVideoPlayer::stateChanged(int state)
{
    Q_ASSERT(QThread::currentThread() == qApp->thread());

    qDebug("state change %d", state);
    if (state == VideoPlayerBackend::Playing)
    {
        m_playBtn->setText(QLatin1String("||"));
        m_posTimer.start(30);
    }
    else
    {
        m_playBtn->setText(QString(QChar(0x25BA)));
        m_posTimer.stop();
        updatePosition();
    }
}

void EventVideoPlayer::durationChanged(qint64 nsDuration)
{
    Q_ASSERT(QThread::currentThread() == qApp->thread());

    if (nsDuration == -1)
        nsDuration = backend.duration();

    /* Time is assumed to be nanoseconds; convert to milliseconds */
    int duration = int(nsDuration / 1000000);
    /* BUG: Shouldn't mindlessly chop to int */
    m_seekSlider->blockSignals(true);
    m_seekSlider->setMaximum(duration);
    m_seekSlider->blockSignals(false);
    updatePosition();
}

void EventVideoPlayer::updatePosition()
{
    Q_ASSERT(QThread::currentThread() == qApp->thread());

    if (!m_seekSlider->maximum())
    {
        qint64 nsDuration = backend.duration();
        if (nsDuration && int(nsDuration / 1000000))
        {
            durationChanged(nsDuration);
            return;
        }
    }

    qint64 nsPosition = backend.position();
    int position = int(nsPosition / 1000000);
    m_seekSlider->blockSignals(true);
    m_seekSlider->setValue(position);
    m_seekSlider->blockSignals(false);

    int secs = nsPosition / 1000000000;
    int durationSecs = backend.duration() / 1000000000;

    m_posText->setText(QString::fromLatin1("%1:%2 / %3:%4").arg(secs / 60, 2, 10, QLatin1Char('0'))
                       .arg(secs % 60, 2, 10, QLatin1Char('0'))
                       .arg(durationSecs / 60, 2, 10, QLatin1Char('0'))
                       .arg(durationSecs % 60, 2, 10, QLatin1Char('0')));
}

void EventVideoPlayer::saveVideo(const QString &path)
{
    if (path.isEmpty())
    {
        bool restart = false;
        if (backend.state() == VideoPlayerBackend::Playing)
        {
            backend.pause();
            restart = true;
        }

        QString upath = QFileDialog::getSaveFileName(this, tr("Save event video"), QString(),
                                     tr("Matroska Video (*.mkv)"));
        if (!upath.isEmpty())
            saveVideo(upath);

        if (restart)
            backend.play();
        return;
    }

    EventVideoDownload *dl = new EventVideoDownload(bcApp->mainWindow);
    dl->setFilePath(path);
    dl->setVideoBuffer(backend.videoBuffer());
    dl->start(bcApp->mainWindow);
}

void EventVideoPlayer::videoContextMenu(const QPoint &rpos)
{
    QPoint pos = rpos;
    if (qobject_cast<QWidget*>(sender()))
        pos = static_cast<QWidget*>(sender())->mapToGlobal(pos);

    QMenu menu(qobject_cast<QWidget*>(sender()));

    if (backend.state() == VideoPlayerBackend::Playing)
        menu.addAction(tr("&Pause"), this, SLOT(playPause()));
    else
        menu.addAction(tr("&Play"), this, SLOT(playPause()));

    menu.addAction(tr("&Restart"), this, SLOT(restart()));

    menu.addSeparator();

    if (m_videoWidget->isFullScreen())
        menu.addAction(tr("Exit &full screen"), m_videoWidget, SLOT(toggleFullScreen()));
    else
        menu.addAction(tr("&Full screen"), m_videoWidget, SLOT(toggleFullScreen()));

    menu.addSeparator();

    menu.addAction(tr("Save video"), this, SLOT(saveVideo()));

    menu.exec(pos);
}