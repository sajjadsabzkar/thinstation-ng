#include "desktop.h"
#include "registry.h"

#include <QApplication>
#include <QCollator>
#include <QDateTime>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QRadialGradient>
#include <QScreen>
#include <QTextLayout>
#include <QTimer>

#include <algorithm>

// One launcher: the icon, and up to two lines of label under it.
static const int kTileW = 128;
static const int kTileH = 112;
static const int kIcon  = 56;
static const int kMargin = 14;
// tp-taskbar's height: the column of icons stops above it.
static const int kTaskbar = 46;

Desktop::Desktop(Registry *reg, QWidget *parent)
    : QWidget(parent), m_reg(reg), m_selected(-1)
{
    setWindowTitle(QLatin1String("tp-desktop"));
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_X11NetWmWindowTypeDesktop, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);

    // The registry is saved by writing a new file over the old one, which a
    // file watch loses track of; the directory watch catches that, and the
    // timer catches anything both miss.
    m_watcher = new QFileSystemWatcher(this);
    const QString user = m_reg->userPath();
    if (QFileInfo::exists(user))
        m_watcher->addPath(user);
    m_watcher->addPath(QFileInfo(user).absolutePath());
    connect(m_watcher, SIGNAL(fileChanged(QString)), this, SLOT(reload()));
    connect(m_watcher, SIGNAL(directoryChanged(QString)), this, SLOT(reload()));

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(reload()));
    timer->start(5000);

    if (QScreen *screen = QApplication::primaryScreen())
        connect(screen, SIGNAL(geometryChanged(QRect)), this, SLOT(fitToScreen()));

    fitToScreen();
    reload();
}

void Desktop::fitToScreen()
{
    QScreen *screen = QApplication::primaryScreen();
    setGeometry(screen ? screen->geometry() : QRect(0, 0, 1024, 768));
}

void Desktop::resizeEvent(QResizeEvent *)
{
    // Size changed: the scaled wallpaper and the icon column are both stale.
    m_wallpaperKey.clear();
    loadWallpaper();
    layoutIcons();
}

void Desktop::reload()
{
    m_reg->load();

    // A save that replaced the file drops it from the watch list.
    const QString user = m_reg->userPath();
    if (QFileInfo::exists(user) && !m_watcher->files().contains(user))
        m_watcher->addPath(user);

    loadWallpaper();
    loadConnections();
    update();
}

// --- wallpaper --------------------------------------------------------------

void Desktop::loadWallpaper()
{
    const QString mode  = m_reg->value(QLatin1String("root/background/mode"),
                                       QLatin1String("colour"));
    const QString colour = m_reg->value(QLatin1String("root/background/colour"),
                                        QLatin1String("#1F2933"));
    const QString image = m_reg->value(QLatin1String("root/background/image"));
    const QString fit   = m_reg->value(QLatin1String("root/background/imageMode"),
                                       QLatin1String("scale"));

    const QFileInfo info(image);
    const QString key = (QStringList()
        << mode << colour << image << fit
        << QString::number(info.exists() ? info.lastModified().toSecsSinceEpoch() : 0)
        << QString::number(width()) << QString::number(height()))
        .join(QLatin1Char('|'));
    if (key == m_wallpaperKey && !m_wallpaper.isNull())
        return;
    m_wallpaperKey = key;

    QColor base(colour);
    if (!base.isValid())
        base = QColor(0x1F, 0x29, 0x33);

    QPixmap canvas(size().isEmpty() ? QSize(1024, 768) : size());
    canvas.fill(base);

    QImage picture;
    if (mode == QLatin1String("image") && !image.isEmpty())
        picture.load(image);

    if (!picture.isNull()) {
        QPainter p(&canvas);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QSize area = canvas.size();
        if (fit == QLatin1String("tile")) {
            p.drawTiledPixmap(canvas.rect(), QPixmap::fromImage(picture));
        } else if (fit == QLatin1String("centre") || fit == QLatin1String("center")) {
            p.drawImage((area.width() - picture.width()) / 2,
                        (area.height() - picture.height()) / 2, picture);
        } else {
            // "scale" fills the screen and crops the overhang, the way a
            // photograph is put behind a desktop; stretching would distort
            // it on anything that is not 16:9.
            const QImage scaled = picture.scaled(area, Qt::KeepAspectRatioByExpanding,
                                                 Qt::SmoothTransformation);
            p.drawImage((area.width() - scaled.width()) / 2,
                        (area.height() - scaled.height()) / 2, scaled);
        }
    }
    m_wallpaper = canvas;
}

// --- connections ------------------------------------------------------------

void Desktop::loadConnections()
{
    QProcess p;
    p.start(QLatin1String("tpreg"), QStringList() << QLatin1String("connections"));
    if (!p.waitForFinished(4000))
        return;
    const QString out = QString::fromUtf8(p.readAllStandardOutput());
    if (out == m_connectionsKey)
        return;
    m_connectionsKey = out;

    const QString selectedUuid = (m_selected >= 0 && m_selected < m_icons.size())
        ? m_icons.at(m_selected).uuid : QString();

    m_icons.clear();
    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        DesktopIcon icon;
        icon.uuid  = line.section(QLatin1Char(' '), 0, 0);
        icon.type  = line.section(QLatin1Char(' '), 1, 1);
        icon.label = line.section(QLatin1Char(' '), 2).trimmed();
        if (icon.uuid.isEmpty())
            continue;
        if (icon.label.isEmpty())
            icon.label = icon.type;
        m_icons.append(icon);
    }

    // ThinPro orders its desktop by name.
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(m_icons.begin(), m_icons.end(),
              [&collator](const DesktopIcon &a, const DesktopIcon &b) {
                  return collator.compare(a.label, b.label) < 0;
              });

    m_selected = -1;
    for (int i = 0; i < m_icons.size(); ++i)
        if (m_icons.at(i).uuid == selectedUuid)
            m_selected = i;

    layoutIcons();
}

void Desktop::layoutIcons()
{
    // Down the left edge first, then the next column, as ThinPro does.
    const int usable = qMax(kTileH, height() - kTaskbar - 2 * kMargin);
    const int perColumn = qMax(1, usable / kTileH);
    for (int i = 0; i < m_icons.size(); ++i) {
        const int col = i / perColumn;
        const int row = i % perColumn;
        m_icons[i].rect = QRect(kMargin + col * kTileW, kMargin + row * kTileH,
                                kTileW, kTileH);
    }
}

int Desktop::iconAt(const QPoint &pos) const
{
    for (int i = 0; i < m_icons.size(); ++i)
        if (m_icons.at(i).rect.contains(pos))
            return i;
    return -1;
}

void Desktop::launch(int index)
{
    if (index < 0 || index >= m_icons.size())
        return;
    // The same entry point the connection manager uses: tp-launch looks
    // the connection up and hands it to the right client.
    QProcess::startDetached(QLatin1String("tp-launch"),
                            QStringList() << m_icons.at(index).uuid);
}

// --- input ------------------------------------------------------------------

void Desktop::mousePressEvent(QMouseEvent *event)
{
    const int hit = iconAt(event->pos());
    if (hit != m_selected) {
        m_selected = hit;
        update();
    }
}

void Desktop::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        launch(iconAt(event->pos()));
}

void Desktop::keyPressEvent(QKeyEvent *event)
{
    if (m_icons.isEmpty())
        return;
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        launch(m_selected);
        return;
    case Qt::Key_Down:
    case Qt::Key_Right:
        m_selected = qMin(m_icons.size() - 1, m_selected + 1);
        break;
    case Qt::Key_Up:
    case Qt::Key_Left:
        m_selected = qMax(0, m_selected - 1);
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    update();
}

// --- painting ---------------------------------------------------------------

// At most two lines, the second cut short with an ellipsis, the way the
// file manager on ThinPro shows "New VMware Horizon View...".
static QStringList wrapLabel(const QString &text, const QFont &font, int width)
{
    QStringList lines;
    QTextLayout layout(text, font);
    layout.beginLayout();
    for (;;) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(width);
        if (lines.size() == 1) {
            // Second line: take everything left and elide it.
            const QString rest = text.mid(line.textStart()).trimmed();
            lines << QFontMetrics(font).elidedText(rest, Qt::ElideRight, width);
            break;
        }
        lines << text.mid(line.textStart(), line.textLength()).trimmed();
    }
    layout.endLayout();
    return lines;
}

void Desktop::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.drawPixmap(0, 0, m_wallpaper);
    p.setRenderHint(QPainter::Antialiasing, true);

    QFont labelFont = font();
    labelFont.setPixelSize(13);
    const QFontMetrics fm(labelFont);

    for (int i = 0; i < m_icons.size(); ++i) {
        const DesktopIcon &icon = m_icons.at(i);
        const QRect tile = icon.rect.adjusted(4, 2, -4, -2);

        if (i == m_selected) {
            p.setPen(QColor(255, 255, 255, 110));
            p.setBrush(QColor(255, 255, 255, 55));
            p.drawRoundedRect(tile, 4, 4);
        }

        const QRect art(tile.center().x() - kIcon / 2, tile.top() + 6, kIcon, kIcon);
        drawTypeIcon(p, art, icon.type);

        p.setFont(labelFont);
        const QStringList lines = wrapLabel(icon.label, labelFont, tile.width() - 8);
        int y = art.bottom() + 8;
        for (int l = 0; l < lines.size(); ++l) {
            const QRect row(tile.left(), y, tile.width(), fm.height());
            // A soft shadow keeps white text readable on any wallpaper.
            p.setPen(QColor(0, 0, 0, 200));
            p.drawText(row.translated(1, 1), Qt::AlignHCenter | Qt::AlignTop, lines.at(l));
            p.setPen(Qt::white);
            p.drawText(row, Qt::AlignHCenter | Qt::AlignTop, lines.at(l));
            y += fm.height();
        }
    }
}

// Every client gets a drawn icon rather than an image file: the image
// ships no icon theme, and a few shapes cost nothing. They are our own
// drawings, not the vendors' marks -- the idea (a monitor, a cloud, a
// globe) is what tells them apart.
static void drawMonitor(QPainter &p, const QRect &r, const QColor &frame,
                        const QColor &top, const QColor &bottom)
{
    const QRectF body(r.x() + 3, r.y() + 5, r.width() - 6, r.height() * 0.66);
    p.setPen(Qt::NoPen);
    p.setBrush(frame);
    p.drawRoundedRect(body, 4, 4);

    QLinearGradient glass(body.topLeft(), body.bottomLeft());
    glass.setColorAt(0, top);
    glass.setColorAt(1, bottom);
    p.setBrush(glass);
    p.drawRoundedRect(body.adjusted(3, 3, -3, -3), 2, 2);

    const qreal cx = body.center().x();
    p.setBrush(frame.darker(115));
    p.drawRect(QRectF(cx - 4, body.bottom(), 8, 6));
    p.drawRoundedRect(QRectF(cx - 13, body.bottom() + 6, 26, 4), 2, 2);
}

static void drawCloud(QPainter &p, const QPointF &c, qreal s, const QColor &colour)
{
    QPainterPath cloud;
    // Winding, not the default odd-even: where the puffs overlap, odd-even
    // cuts holes and the cloud shows seams.
    cloud.setFillRule(Qt::WindingFill);
    cloud.addEllipse(QPointF(c.x() - s * 0.35, c.y() + s * 0.05), s * 0.28, s * 0.24);
    cloud.addEllipse(QPointF(c.x() + s * 0.05, c.y() - s * 0.12), s * 0.34, s * 0.32);
    cloud.addEllipse(QPointF(c.x() + s * 0.40, c.y() + s * 0.07), s * 0.24, s * 0.21);
    cloud.addRoundedRect(QRectF(c.x() - s * 0.62, c.y(), s * 1.24, s * 0.30), s * 0.15, s * 0.15);
    p.setPen(Qt::NoPen);
    p.setBrush(colour);
    p.drawPath(cloud.simplified());
}

void Desktop::drawTypeIcon(QPainter &p, const QRect &r, const QString &type)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    if (type == QLatin1String("horizon") || type == QLatin1String("vmview")) {
        // Desktop virtualisation: a green screen with a cloud on it.
        drawMonitor(p, r, QColor(0x3C, 0x8C, 0x1E), QColor(0x8F, 0xD1, 0x4F),
                    QColor(0x5A, 0xA8, 0x2A));
        // Centred on the screen part of the monitor, and big enough to
        // read as a cloud at 56 px.
        drawCloud(p, QPointF(r.center().x(), r.y() + 5 + r.height() * 0.35),
                  r.width() * 0.52, Qt::white);
    } else if (type == QLatin1String("ica") || type == QLatin1String("citrix")) {
        // Citrix: a blue disc with white rings spiralling in.
        const QRectF disc = QRectF(r).adjusted(4, 4, -4, -4);
        QRadialGradient g(disc.center(), disc.width() / 2);
        g.setColorAt(0, QColor(0x3D, 0x9B, 0xF0));
        g.setColorAt(1, QColor(0x14, 0x5F, 0xC8));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(disc);
        QPen ring(Qt::white, r.width() / 14.0, Qt::SolidLine, Qt::RoundCap);
        p.setPen(ring);
        p.setBrush(Qt::NoBrush);
        for (int k = 0; k < 3; ++k) {
            const qreal inset = disc.width() * (0.18 + k * 0.12);
            p.drawArc(disc.adjusted(inset, inset, -inset, -inset),
                      (60 + k * 40) * 16, (250 - k * 30) * 16);
        }
    } else if (type == QLatin1String("firefox") || type == QLatin1String("chrome")
               || type == QLatin1String("browser")) {
        // The web: a globe.
        const QRectF globe = QRectF(r).adjusted(5, 5, -5, -5);
        QRadialGradient g(globe.center() - QPointF(globe.width() * 0.2, globe.height() * 0.2),
                          globe.width() * 0.75);
        g.setColorAt(0, QColor(0x8F, 0xD8, 0xF5));
        g.setColorAt(1, QColor(0x1D, 0x5F, 0xA8));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(globe);
        p.setPen(QPen(QColor(255, 255, 255, 170), 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(globe.adjusted(globe.width() * 0.28, 0, -globe.width() * 0.28, 0));
        p.drawLine(QPointF(globe.center().x(), globe.top()),
                   QPointF(globe.center().x(), globe.bottom()));
        p.drawLine(QPointF(globe.left(), globe.center().y()),
                   QPointF(globe.right(), globe.center().y()));
        const qreal band = globe.height() * 0.24;
        p.drawLine(QPointF(globe.left() + 4, globe.center().y() - band),
                   QPointF(globe.right() - 4, globe.center().y() - band));
        p.drawLine(QPointF(globe.left() + 4, globe.center().y() + band),
                   QPointF(globe.right() - 4, globe.center().y() + band));
    } else if (type == QLatin1String("freerdp") || type == QLatin1String("rdesktop")) {
        // Remote desktop: a plain monitor with a blue screen.
        drawMonitor(p, r, QColor(0x3A, 0x40, 0x48), QColor(0x9B, 0xDD, 0xFF),
                    QColor(0x1E, 0x7F, 0xD8));
    } else {
        drawMonitor(p, r, QColor(0x55, 0x5B, 0x63), QColor(0xD5, 0xD9, 0xDE),
                    QColor(0x8A, 0x92, 0x9B));
    }
    p.restore();
}
