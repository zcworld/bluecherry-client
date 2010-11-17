#include "CameraAreaWidget.h"
#include "LiveFeedWidget.h"
#include "core/BluecherryApp.h"
#include "core/DVRServer.h"
#include <QGridLayout>
#include <QDataStream>
#include <QDragEnterEvent>
#include <QMimeData>

CameraAreaWidget::CameraAreaWidget(QWidget *parent)
    : QFrame(parent), m_rowCount(0), m_columnCount(0)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFrameStyle(QFrame::Sunken | QFrame::Panel);
    setAutoFillBackground(true);
    setAcceptDrops(true);

    QPalette p = palette();
    p.setColor(QPalette::Window, QColor(20, 20, 20));
    p.setColor(QPalette::WindowText, Qt::white);
    setPalette(p);

    mainLayout = new QGridLayout(this);
    mainLayout->setMargin(0);
    mainLayout->setSpacing(2);

    setGridSize(3, 3);
}

void CameraAreaWidget::setGridSize(int rows, int columns)
{
    rows = qMax(0, rows);
    columns = qMax(0, columns);
    if (rows == m_rowCount && columns == m_columnCount)
        return;

    Q_ASSERT(m_cameraWidgets.size() == m_rowCount);
    Q_ASSERT(m_cameraWidgets.isEmpty() || m_cameraWidgets[0].size() == m_columnCount);

    if (m_rowCount > rows)
    {
        int remove = m_rowCount - rows;

        /* If there are any empty rows, remove those first */
        for (int r = 0; r < m_rowCount; ++r)
        {
            const QList<LiveFeedWidget*> &row = m_cameraWidgets[r];
            bool empty = true;
            foreach (LiveFeedWidget *widget, row)
            {
                if (widget->camera())
                {
                    empty = false;
                    break;
                }
            }

            if (empty)
            {
                removeRow(r);
                if (!--remove)
                    break;
                --r;
            }
        }

        /* Otherwise, take rows from the bottom */
        for (int r = m_cameraWidgets.size()-1; remove && r >= 0; --r, --remove)
            removeRow(r);

        Q_ASSERT(!remove);
    }

    if (m_columnCount > columns)
    {
        int remove = m_columnCount - columns;

        for (int c = 0; c < m_columnCount; ++c)
        {
            bool empty = true;
            for (QList<QList<LiveFeedWidget*> >::Iterator it = m_cameraWidgets.begin(); it != m_cameraWidgets.end(); ++it)
            {
                if (it->at(c)->camera())
                {
                    empty = false;
                    break;
                }
            }

            if (empty)
            {
                removeColumn(c);
                if (!--remove)
                    break;
                --c;
            }
        }

        /* removeColumn() has updated m_columnCount */

        for (int c = m_columnCount-1; remove && c >= 0; --c, --remove)
            removeColumn(c);

        Q_ASSERT(!remove);
    }

    while (m_columnCount < columns)
    {
        for (int r = 0; r < m_cameraWidgets.size(); ++r)
        {
            LiveFeedWidget *widget = new LiveFeedWidget;
            connect(widget, SIGNAL(cameraChanged(DVRCamera)), SLOT(onCameraChanged()));
            m_cameraWidgets[r].append(widget);
            mainLayout->addWidget(widget, r, m_columnCount);
        }

        ++m_columnCount;
    }

    while (m_rowCount < rows)
    {
        QList<LiveFeedWidget*> row;
        for (int c = 0; c < m_columnCount; ++c)
        {
            LiveFeedWidget *widget = new LiveFeedWidget;
            connect(widget, SIGNAL(cameraChanged(DVRCamera)), SLOT(onCameraChanged()));
            row.append(widget);
            mainLayout->addWidget(widget, m_rowCount, c);
        }
        m_cameraWidgets.append(row);
        ++m_rowCount;
    }

    m_rowCount = rows;
    m_columnCount = columns;

    Q_ASSERT(m_cameraWidgets.size() == m_rowCount);
    Q_ASSERT(m_cameraWidgets.isEmpty() || m_cameraWidgets[0].size() == m_columnCount);

    m_dragWidgets.clear();

    emit gridSizeChanged(rows, columns);
}

void CameraAreaWidget::removeColumn(int c)
{
    for (int r = 0; r < m_cameraWidgets.size(); ++r)
    {
        delete m_cameraWidgets[r].takeAt(c);

        for (int i = c; i < m_cameraWidgets[r].size(); ++i)
            mainLayout->addWidget(m_cameraWidgets[r][i], r, i);
    }

    m_columnCount--;
}

void CameraAreaWidget::removeRow(int r)
{
    qDeleteAll(m_cameraWidgets.takeAt(r));

    for (int i = r; i < m_cameraWidgets.size(); ++i)
    {
        for (int c = 0; c < m_cameraWidgets[i].size(); ++c)
            mainLayout->addWidget(m_cameraWidgets[i][c], i, c);
    }

    m_rowCount--;
}

QByteArray CameraAreaWidget::saveLayout() const
{
    QByteArray re;
    QDataStream data(&re, QIODevice::WriteOnly);
    data.setVersion(QDataStream::Qt_4_5);

    data << m_rowCount << m_columnCount;
    for (int r = 0; r < m_rowCount; ++r)
        for (int c = 0; c < m_columnCount; ++c)
            data << *m_cameraWidgets[r][c];

    return re;
}

bool CameraAreaWidget::loadLayout(const QByteArray &buf)
{
    if (buf.isEmpty())
        return false;

    QDataStream data(buf);
    data.setVersion(QDataStream::Qt_4_5);

    int rc, cc;
    data >> rc >> cc;
    if (data.status() != QDataStream::Ok)
        return false;

    setGridSize(rc, cc);

    for (int r = 0; r < m_rowCount; ++r)
        for (int c = 0; c < m_columnCount; ++c)
            data >> *m_cameraWidgets[r][c];

    return (data.status() == QDataStream::Ok);
}

void CameraAreaWidget::openFullScreen()
{
    setWindowFlags(Qt::Window);
    setFrameStyle(QFrame::NoFrame);
    showFullScreen();
}

void CameraAreaWidget::closeFullScreen()
{
    setWindowFlags(0);
    setFrameStyle(QFrame::Sunken | QFrame::Panel);
    show();
}

void CameraAreaWidget::toggleFullScreen()
{
    if (isFullScreen())
        closeFullScreen();
    else
        openFullScreen();
}

void CameraAreaWidget::onCameraChanged()
{
    emit cameraChanged(qobject_cast<LiveFeedWidget*>(sender()));
}

void CameraAreaWidget::addCamera(const DVRCamera &camera)
{
    /* Set the camera in the first unused space. Add a row/column if necessary. */
    foreach (const QList<LiveFeedWidget*> &row, m_cameraWidgets)
    {
        foreach (LiveFeedWidget *w, row)
        {
            if (!w->camera())
            {
                w->setCamera(camera);
                return;
            }
        }
    }

    /* Add a row or a column to make space */
    if (m_columnCount < m_rowCount)
    {
        /* Add column */
        addColumn();
        if (m_rowCount < 1)
            addRow();
        m_cameraWidgets[0][m_columnCount-1]->setCamera(camera);
    }
    else
    {
        /* Add row */
        addRow();
        if (m_columnCount < 1)
            addColumn();
        m_cameraWidgets[m_rowCount-1][0]->setCamera(camera);
    }
}

void CameraAreaWidget::autoFill()
{
    QSet<DVRCamera> existing;
    int available = 0;
    foreach (const QList<LiveFeedWidget*> &row, m_cameraWidgets)
    {
        foreach (LiveFeedWidget *w, row)
        {
            if (w->camera())
                existing.insert(w->camera());
            else
                ++available;
        }
    }

    if (!available)
        return;

    int pRow = 0, pCol = 0;

    foreach (DVRServer *server, bcApp->servers())
    {
        if (!server->api->isOnline())
            continue;

        foreach (const DVRCamera &camera, server->cameras())
        {
            if (!existing.contains(camera) && camera.canStream())
            {
                for (; pRow < m_rowCount; pRow++)
                {
                    for (; pCol < m_columnCount; pCol++)
                    {
                        if (!m_cameraWidgets[pRow][pCol]->camera())
                        {
                            m_cameraWidgets[pRow][pCol]->setCamera(camera);
                            pCol++;
                            if (!--available)
                                return;
                            goto nextCamera;
                        }
                    }

                    pCol = 0;
                }
            }

nextCamera:
            ;
        }
    }
}

void CameraAreaWidget::dragEnterEvent(QDragEnterEvent *ev)
{
    if (ev->mimeData()->hasFormat(QLatin1String("application/x-bluecherry-dvrcamera")))
    {
        ev->acceptProposedAction();
    }
}

void CameraAreaWidget::dragLeaveEvent(QDragLeaveEvent *ev)
{
    Q_UNUSED(ev);

    foreach (LiveFeedWidget *w, m_dragWidgets)
        w->endDrag();
    m_dragWidgets.clear();
}

void CameraAreaWidget::dragMoveEvent(QDragMoveEvent *ev)
{
    if (!ev->mimeData()->hasFormat(QLatin1String("application/x-bluecherry-dvrcamera")))
        return;

    LiveFeedWidget *fw = qobject_cast<LiveFeedWidget*>(childAt(ev->pos()));
    if (!fw)
        return;

    if (!m_dragWidgets.isEmpty() && m_dragWidgets[0] == fw)
        return;

    foreach (LiveFeedWidget *w, m_dragWidgets)
        w->endDrag();
    m_dragWidgets.clear();

    QList<DVRCamera> cameras = DVRCamera::fromMimeData(ev->mimeData());
    if (cameras.isEmpty())
        return;

    bool found = false;
    foreach (const QList<LiveFeedWidget*> &row, m_cameraWidgets)
    {
        foreach (LiveFeedWidget *w, row)
        {
            if (w == fw)
                found = true;

            if (found)
            {
                w->beginDrag(cameras.takeFirst());
                m_dragWidgets.append(w);
                if (cameras.isEmpty())
                goto end;
            }
        }
    }

end:
    ev->accept(fw->geometry());
}

void CameraAreaWidget::dropEvent(QDropEvent *ev)
{
    foreach (LiveFeedWidget *w, m_dragWidgets)
        w->endDrag(true);
    m_dragWidgets.clear();
    ev->acceptProposedAction();
}

void CameraAreaWidget::mousePressEvent(QMouseEvent *ev)
{
    LiveFeedWidget *fw = qobject_cast<LiveFeedWidget*>(childAt(ev->pos()));
    if (!fw || ev->button() != Qt::LeftButton)
    {
        ev->ignore();
        return;
    }

    ev->accept();

    Q_ASSERT(m_dragWidgets.isEmpty());
    m_dragWidgets.clear();
    m_dragWidgets.append(fw);
}

void CameraAreaWidget::mouseMoveEvent(QMouseEvent *ev)
{
    if (!(ev->buttons() & Qt::LeftButton) || m_dragWidgets.isEmpty())
        return;

    Q_ASSERT(m_dragWidgets.size() == 1);

    LiveFeedWidget *overWidget = qobject_cast<LiveFeedWidget*>(childAt(ev->pos()));
    if (!overWidget || overWidget == m_dragWidgets[0])
        return;

    int dragRow, dragCol, overRow, overCol, dummy;

    int dragPos = mainLayout->indexOf(m_dragWidgets[0]);
    int overPos = mainLayout->indexOf(overWidget);
    if (dragPos < 0 || overPos < 0)
        return;

    mainLayout->getItemPosition(dragPos, &dragRow, &dragCol, &dummy, &dummy);
    mainLayout->getItemPosition(overPos, &overRow, &overCol, &dummy, &dummy);
    mainLayout->addWidget(overWidget, dragRow, dragCol);
    mainLayout->addWidget(m_dragWidgets[0], overRow, overCol);

    /* Swap places in the list */
    m_cameraWidgets[dragRow][dragCol] = overWidget;
    m_cameraWidgets[overRow][overCol] = m_dragWidgets[0];

    Q_ASSERT(mainLayout->indexOf(overWidget) >= 0);
    Q_ASSERT(mainLayout->indexOf(m_dragWidgets[0]) >= 0);
}

void CameraAreaWidget::mouseReleaseEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton)
        m_dragWidgets.clear();
}
