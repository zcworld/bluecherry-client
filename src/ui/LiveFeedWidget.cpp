#include "LiveFeedWidget.h"
#include "core/DVRCamera.h"
#include "core/DVRServer.h"
#include "core/MJpegStream.h"
#include <QSettings>
#include <QPainter>
#include <QPaintEvent>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QToolTip>
#include <QDataStream>

LiveFeedWidget::LiveFeedWidget(QWidget *parent)
    : QWidget(parent), m_stream(0), m_titleHeight(-1), m_isPaused(false)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::ClickFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setContextMenuPolicy(Qt::DefaultContextMenu);

    QPalette p = palette();
    p.setColor(QPalette::Window, Qt::black);
    setPalette(p);

    QFont f = font();
    f.setBold(true);
    setFont(f);

    setStatusMessage(tr("No\nCamera"));
}

LiveFeedWidget::~LiveFeedWidget()
{
    if (m_stream)
    {
        m_stream->disconnect(this);
        m_stream->updateScaleSizes();
    }
}

void LiveFeedWidget::clone(LiveFeedWidget *other)
{
    setCamera(other->camera());
    m_statusMsg = other->statusMessage();
    m_currentFrame = other->m_currentFrame;
}

void LiveFeedWidget::setCamera(const DVRCamera &camera)
{
    if (camera == m_camera)
        return;

    if (m_camera)
        disconnect(m_camera, 0, this, 0);

    if (m_stream)
    {
        m_stream->disconnect(this);
        m_stream->updateScaleSizes();
        m_stream.clear();
    }

    m_camera = camera;
    m_currentFrame = QPixmap();
    m_statusMsg.clear();
    m_isPaused = false;

    updateGeometry();
    update();

    if (!m_camera)
    {
        setStatusMessage(tr("No\nCamera"));
    }
    else
    {
        connect(m_camera, SIGNAL(dataUpdated()), SLOT(cameraDataUpdated()));
        setStream(m_camera.mjpegStream());
    }

    emit cameraChanged(m_camera);
}

void LiveFeedWidget::cameraDataUpdated()
{
    QSharedPointer<MJpegStream> nstream = m_camera.mjpegStream();
    if (m_stream != nstream)
        setStream(nstream);

    update();
}

void LiveFeedWidget::setStream(const QSharedPointer<MJpegStream> &stream)
{
    if (isPaused() && !stream.isNull())
        return;

    if (m_stream)
        m_stream.data()->disconnect(this);

    clearStatusMessage();
    m_stream = stream;

    if (m_stream)
    {
        QPixmap streamFrame = m_stream->currentFrame();
        if (!streamFrame.isNull() || m_currentFrame.isNull())
            m_currentFrame = streamFrame;

        connect(m_stream.data(), SIGNAL(updateFrame(QPixmap,QVector<QImage>)),
                SLOT(updateFrame(QPixmap,QVector<QImage>)));
        connect(m_stream.data(), SIGNAL(buildScaleSizes(QVector<QSize>&)), SLOT(addScaleSize(QVector<QSize>&)));
        connect(m_stream.data(), SIGNAL(stateChanged(int)), SLOT(mjpegStateChanged(int)));
        connect(m_stream.data(), SIGNAL(streamSizeChanged(QSize)), SLOT(streamSizeChanged(QSize)));
        m_stream->start();

        if (!m_stream->streamSize().isEmpty())
            streamSizeChanged(m_stream->streamSize());
    }
    else if (!isPaused())
        setStatusMessage(tr("No\nVideo"));

    update();
}

void LiveFeedWidget::setPaused(bool paused)
{
    if (!m_camera)
        return;

    if (paused && !m_isPaused)
    {
        m_isPaused = true;
        setStream(QSharedPointer<MJpegStream>());
    }
    else
    {
        m_isPaused = false;
        setStream(m_camera.mjpegStream());
#if 0
        m_isPaused = false;
        m_currentFrame = m_stream->currentFrame();
        if (m_currentFrame.isNull())
            mjpegStateChanged(m_stream->state());
        update();
#endif
    }
}

void LiveFeedWidget::setStatusMessage(const QString &message)
{
    m_statusMsg = message;
    update();
}

void LiveFeedWidget::setWindow()
{
    setAcceptDrops(true);
    setWindowFlags(Qt::Window);
}

QWidget *LiveFeedWidget::openWindow()
{
    LiveFeedWidget *widget = new LiveFeedWidget(window());
    widget->setWindow();
    widget->setAttribute(Qt::WA_DeleteOnClose);
    widget->clone(this);
    widget->show();
    return widget;
}

void LiveFeedWidget::closeCamera()
{
    if (isWindow())
        close();
    else
        clearCamera();
}

void LiveFeedWidget::setFullScreen(bool on)
{
    if (on)
    {
        if (isWindow())
            showFullScreen();
        else
            openWindow()->showFullScreen();
    }
    else if (isWindow())
        close();
}

void LiveFeedWidget::addScaleSize(QVector<QSize> &sizes)
{
    sizes.append(imageArea().size());
}

void LiveFeedWidget::updateFrame(const QPixmap &frame, const QVector<QImage> &scaledFrames)
{
    if (m_isPaused)
        return;

    QSize desired = frame.size();
    desired.scale(imageArea().size(), Qt::KeepAspectRatio);

    m_currentFrame = frame;
    for (QVector<QImage>::ConstIterator it = scaledFrames.begin(); it != scaledFrames.end(); ++it)
    {
        if (it->size() == desired)
        {
            m_currentFrame = QPixmap::fromImage(*it);
            break;
        }
    }

    if (m_stream && m_stream->state() == MJpegStream::Streaming)
        m_statusMsg.clear();
    update();
}

void LiveFeedWidget::mjpegStateChanged(int state)
{
    if (m_isPaused)
        return;

    Q_ASSERT(m_stream);
    setToolTip(QString());

    switch (state)
    {
    case MJpegStream::Error:
        setStatusMessage(tr("Stream Error"));
        setToolTip(m_stream->errorMessage());
        break;
    case MJpegStream::StreamOffline:
        setStatusMessage(tr("Server\nOffline"));
        break;
    case MJpegStream::NotConnected:
        setStatusMessage(tr("Disconnected"));
        break;
    case MJpegStream::Connecting:
        setStatusMessage(tr("Connecting..."));
        break;
    case MJpegStream::Streaming:
        setStatusMessage(tr("Buffering..."));
        break;
    default:
        clearStatusMessage();
        break;
    }
}

void LiveFeedWidget::streamSizeChanged(const QSize &size)
{
    if (!size.isEmpty() && isWindow() && !isFullScreen() && !testAttribute(Qt::WA_Resized))
        resize(size + QSize(0, m_titleHeight));
}

QSize LiveFeedWidget::sizeHint() const
{
    return QSize();
}

QRect LiveFeedWidget::imageArea() const
{
    ensurePolished();
    return rect().adjusted(0, m_titleHeight, 0, 0);
}

bool LiveFeedWidget::event(QEvent *event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::Polish)
    {
        QFontMetrics fm(font());
        m_titleHeight = qMax(15, fm.height() + 4);
        updateGeometry();
        update();
    }

    return QWidget::event(event);
}

void LiveFeedWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    QRect r = rect();
    p.eraseRect(r);

    /* Title area */
    QRect headerRect(r.left(), r.top(), r.width(), m_titleHeight);

    p.save();

    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(QColor(35, 35, 35), Qt::Dense6Pattern));
    p.drawRect(headerRect);

    QRect titleTextRect;
    p.setPen(QColor(195, 195, 195));
    p.drawText(QRect(r.left(), r.top(), r.width(), m_titleHeight), Qt::AlignCenter,
               (m_dragCamera ? m_dragCamera : m_camera).displayName(), &titleTextRect);

    if (isPaused())
    {
        p.setPen(QColor(255, 144, 0));
        QRect pauseTextRect = headerRect;
        int space = qMax(6, p.fontMetrics().width(QLatin1Char(' ')));
        pauseTextRect.setLeft(titleTextRect.right()+space);
        pauseTextRect.setRight(pauseTextRect.right()-space);
        p.drawText(pauseTextRect, Qt::AlignLeft | Qt::AlignVCenter, tr("PAUSED"));
    }

    p.restore();

    r.setTop(r.top()+m_titleHeight);

    if (m_dragCamera)
    {
        p.save();
        p.setPen(QPen(QBrush(Qt::white), 2));
        p.setRenderHint(QPainter::Antialiasing, true);
        p.drawRoundedRect(r.adjusted(2, 2, -2, -2), 3, 3);
        p.restore();
        return;
    }

    if (!m_currentFrame.isNull())
    {
        QSize renderSize = m_currentFrame.size();
        renderSize.scale(r.size(), Qt::KeepAspectRatio);

        if (renderSize != m_currentFrame.size())
        {
            if (m_stream)
                m_currentFrame = m_stream->currentFrame();
            m_currentFrame = m_currentFrame.scaled(renderSize, Qt::KeepAspectRatio, Qt::FastTransformation);
        }

        QPoint topLeft(r.x() + (r.width() - renderSize.width())/2, r.y() + (r.height() - renderSize.height())/2);
        p.drawPixmap(topLeft, m_currentFrame);
    }

    if (!m_statusMsg.isEmpty())
    {
        /* If the frame was painted, darken it, because status indicates it's not operating normally */
        p.fillRect(r, QColor(0, 0, 0, 190));

        QFont font(p.font());
        font.setPointSize(14);
        font.setBold(false);
        p.save();
        p.setFont(font);
        p.setPen(m_currentFrame.isNull() ? QColor(60, 60, 60) : QColor(Qt::white));

        p.drawText(r, Qt::AlignCenter, m_statusMsg);
        p.restore();
    }
}

DVRCamera LiveFeedWidget::cameraFromMime(const QMimeData *mimeData)
{
    QByteArray data = mimeData->data(QLatin1String("application/x-bluecherry-dvrcamera"));
    QDataStream stream(&data, QIODevice::ReadOnly);

    /* Ignore everything except the first camera dropped */
    int serverid, cameraid;
    stream >> serverid >> cameraid;

    if (stream.status() != QDataStream::Ok)
        return DVRCamera();

    return DVRCamera::getCamera(serverid, cameraid);
}

void LiveFeedWidget::beginDrag(const DVRCamera &c)
{
    m_dragCamera = c;
    update();
}

void LiveFeedWidget::endDrag(bool keep)
{
    if (keep)
        setCamera(m_dragCamera);

    m_dragCamera = DVRCamera();
    update();
}

void LiveFeedWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QLatin1String("application/x-bluecherry-dvrcamera")))
    {
        beginDrag(cameraFromMime(event->mimeData()));
        event->acceptProposedAction();
    }
}

void LiveFeedWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    endDrag();
}

void LiveFeedWidget::dropEvent(QDropEvent *event)
{
    endDrag(true);
    event->acceptProposedAction();
}

void LiveFeedWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    if (m_stream)
        m_stream->updateScaleSizes();
}

void LiveFeedWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    menu.addAction(tr("Snapshot"), this, SLOT(saveSnapshot()))->setEnabled(m_camera && !m_currentFrame.isNull());
    menu.addSeparator();

    QAction *a = menu.addAction(isPaused() ? tr("Paused") : tr("Pause"), this, SLOT(togglePaused()));
    a->setCheckable(true);
    a->setChecked(isPaused());
    a->setEnabled(m_camera && (m_stream || isPaused()));

    menu.addSeparator();
    menu.addAction(tr("Open in window"), this, SLOT(openWindow()));
    menu.addAction(!isFullScreen() ? tr("Open as fullscreen") : tr("Exit fullscreen"), this, SLOT(toggleFullScreen()));
    menu.addSeparator();

    QAction *actClose = menu.addAction(tr("Close camera"), this, SLOT(closeCamera()));
    actClose->setEnabled(m_camera);

    menu.exec(event->globalPos());
}

void LiveFeedWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Escape:
        if (isWindow())
            close();
        break;
    case Qt::Key_F11:
        toggleFullScreen();
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }

    event->accept();
}

void LiveFeedWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !isPaused())
        return;

    setPaused(false);
}

void LiveFeedWidget::saveSnapshot(const QString &ifile)
{
    if (!m_stream)
        return;

    /* Grab the current frame, so the user gets what they expect regardless of the time taken by the dialog */
    QPixmap frame = m_stream->currentFrame();
    if (frame.isNull())
        return;

    QString file = ifile;

    if (file.isEmpty())
    {
        file = QFileDialog::getSaveFileName(this, tr("Save Camera Snapshot"), QString(), tr("Image (*.jpg)"));
        if (file.isEmpty())
            return;
    }

    if (!frame.save(file, "jpeg"))
    {
        QMessageBox::critical(this, tr("Snapshot Error"), tr("An error occurred while saving the snapshot image."),
                              QMessageBox::Ok);
        return;
    }

    QToolTip::showText(mapToGlobal(QPoint(0,0)), tr("Snapshot Saved"), this);
}
