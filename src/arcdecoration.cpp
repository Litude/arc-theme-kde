/*
* Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
* Copyright 2014  Hugo Pereira Da Costa <hugo.pereira@free.fr>
* Copyright 2018  Vlad Zahorodnii <vlad.zahorodnii@kde.org>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of
* the License or (at your option) version 3 or any later version
* accepted by the membership of KDE e.V. (or its successor approved
* by the membership of KDE e.V.), which shall act as a proxy
* defined in Section 14 of version 3 of the license.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "arcdecoration.h"

#include "arc.h"
#include "arcsettingsprovider.h"
#include "config-arc.h"
#include "config/arcconfigwidget.h"

#include "arcbutton.h"

#include "breezeboxshadowrenderer.h"

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/DecorationButtonGroup>
#include <KDecoration2/DecorationSettings>
#include <KDecoration2/DecorationShadow>

#include <KConfigGroup>
#include <KColorUtils>
#include <KSharedConfig>
#include <KPluginFactory>

#include <QPainter>
#include <QTextStream>
#include <QTimer>
#include <QVariantAnimation>

#if BREEZE_HAVE_X11
#include <QX11Info>
#endif

#include <cmath>

K_PLUGIN_FACTORY_WITH_JSON(
    ArcDecoFactory,
    "arc.json",
    registerPlugin<Arc::Decoration>();
    registerPlugin<Arc::Button>(QStringLiteral("button"));
    registerPlugin<Arc::ConfigWidget>(QStringLiteral("kcmodule"));
)

namespace
{
    struct ShadowParams {
        ShadowParams()
            : offset(QPoint(0, 0))
            , radius(0)
            , opacity(0) {}

        ShadowParams(const QPoint &offset, int radius, qreal opacity)
            : offset(offset)
            , radius(radius)
            , opacity(opacity) {}

        QPoint offset;
        int radius;
        qreal opacity;
    };

    struct CompositeShadowParams {
        CompositeShadowParams() = default;

        CompositeShadowParams(
                const QPoint &offset,
                const ShadowParams &shadow1,
                const ShadowParams &shadow2)
            : offset(offset)
            , shadow1(shadow1)
            , shadow2(shadow2) {}

        bool isNone() const {
            return qMax(shadow1.radius, shadow2.radius) == 0;
        }

        QPoint offset;
        ShadowParams shadow1;
        ShadowParams shadow2;
    };

    const CompositeShadowParams s_shadowParams[] = {
        // None
        CompositeShadowParams(),
        // Small
        CompositeShadowParams(
            QPoint(0, 4),
            ShadowParams(QPoint(0, 0), 16, 1),
            ShadowParams(QPoint(0, -2), 8, 0.4)),
        // Medium
        CompositeShadowParams(
            QPoint(0, 8),
            ShadowParams(QPoint(0, 0), 32, 0.9),
            ShadowParams(QPoint(0, -4), 16, 0.3)),
        // Large
        CompositeShadowParams(
            QPoint(0, 12),
            ShadowParams(QPoint(0, 0), 48, 0.8),
            ShadowParams(QPoint(0, -6), 24, 0.2)),
        // Very large
        CompositeShadowParams(
            QPoint(0, 16),
            ShadowParams(QPoint(0, 0), 64, 0.7),
            ShadowParams(QPoint(0, -8), 32, 0.1)),
    };

    inline CompositeShadowParams lookupShadowParams(int size)
    {
        switch (size) {
        case Arc::InternalSettings::ShadowNone:
            return s_shadowParams[0];
        case Arc::InternalSettings::ShadowSmall:
            return s_shadowParams[1];
        case Arc::InternalSettings::ShadowMedium:
            return s_shadowParams[2];
        case Arc::InternalSettings::ShadowLarge:
            return s_shadowParams[3];
        case Arc::InternalSettings::ShadowVeryLarge:
            return s_shadowParams[4];
        default:
            // Fallback to the Large size.
            return s_shadowParams[3];
        }
    }
}

namespace Arc
{

    static const QColor LIGHT_TITLE_FONT_COLOR = QColor { "#f1525d76" };
    static const QColor LIGHT_TITLE_FONT_COLOR_INACTIVE = QColor { "#7f525d76" };
    static const QColor LIGHT_WINDOW_MAIN_BG = QColor { "#e7e8eb" };
    static const QColor LIGHT_WINDOW_MAIN_BORDER = QColor { "#4a000000" };
    static const QColor LIGHT_WINDOW_HIGHLIGHT = QColor { "#eff0f2" };

    static const QColor DARK_TITLE_FONT_COLOR = QColor { "#f1cfdae7" };
    static const QColor DARK_TITLE_FONT_COLOR_INACTIVE = QColor { "#7fcfdae7" };
    static const QColor DARK_WINDOW_MAIN_BG = QColor { "#2f343f" };
    static const QColor DARK_WINDOW_MAIN_BORDER = QColor { "#B9000000" };
    static const QColor DARK_WINDOW_HIGHLIGHT = QColor { "#363b48" };

    //________________________________________________________________
    static int g_sDecoCount = 0;
    static int g_shadowSizeEnum = InternalSettings::ShadowLarge;
    static int g_shadowStrength = 255;
    static QColor g_shadowColor = Qt::black;
    static QSharedPointer<KDecoration2::DecorationShadow> g_sShadow;

    //________________________________________________________________
    Decoration::Decoration(QObject *parent, const QVariantList &args)
        : KDecoration2::Decoration(parent, args)
        , m_animation( new QVariantAnimation( this ) )
    {
        g_sDecoCount++;
    }

    //________________________________________________________________
    Decoration::~Decoration()
    {
        g_sDecoCount--;
        if (g_sDecoCount == 0) {
            // last deco destroyed, clean up shadow
            g_sShadow.clear();
        }

    }

    //________________________________________________________________
    void Decoration::setOpacity( qreal value )
    {
        if( m_opacity == value ) return;
        m_opacity = value;
        update();
    }

    //________________________________________________________________
    QColor Decoration::titleBarColor() const
    {
        return m_internalSettings->arcTheme() == InternalSettings::ThemeDark ? DARK_WINDOW_MAIN_BG : LIGHT_WINDOW_MAIN_BG;
    }

    //________________________________________________________________
    QColor Decoration::outlineColor() const
    {
        return m_internalSettings->arcTheme() == InternalSettings::ThemeDark ? DARK_WINDOW_MAIN_BORDER : LIGHT_WINDOW_MAIN_BORDER;
    }

    //________________________________________________________________
    QColor Decoration::highlightColor() const
    {
        return m_internalSettings->arcTheme() == InternalSettings::ThemeDark ? DARK_WINDOW_HIGHLIGHT : LIGHT_WINDOW_HIGHLIGHT;
    }

    //________________________________________________________________
    QColor Decoration::fontColor() const
    {

        auto clientPtr = client().toStrongRef();
        auto& activeColor = m_internalSettings->arcTheme() == InternalSettings::ThemeDark ? DARK_TITLE_FONT_COLOR : LIGHT_TITLE_FONT_COLOR;
        auto& inactiveColor = m_internalSettings->arcTheme() == InternalSettings::ThemeDark ? DARK_TITLE_FONT_COLOR_INACTIVE : LIGHT_TITLE_FONT_COLOR_INACTIVE;

        if( m_animation->state() == QAbstractAnimation::Running )
        {
            return KColorUtils::mix(inactiveColor, activeColor, m_opacity );
        } else {
            return ( (!clientPtr.isNull() && clientPtr.data()->isActive()) ? activeColor : inactiveColor);
        }

    }

    //________________________________________________________________
    void Decoration::init()
    {
        auto c = client().toStrongRef().data();

        // active state change animation
        // It is important start and end value are of the same type, hence 0.0 and not just 0
        m_animation->setStartValue( 0.0 );
        m_animation->setEndValue( 1.0 );
        m_animation->setEasingCurve( QEasingCurve::InOutQuad );
        connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            setOpacity(value.toReal());
        });

        reconfigure();
        updateTitleBar();
        auto s = settings();
        connect(s.data(), &KDecoration2::DecorationSettings::borderSizeChanged, this, &Decoration::recalculateBorders);

        // a change in font might cause the borders to change
        connect(s.data(), &KDecoration2::DecorationSettings::fontChanged, this, &Decoration::recalculateBorders);
        connect(s.data(), &KDecoration2::DecorationSettings::spacingChanged, this, &Decoration::recalculateBorders);

        // buttons
        connect(s.data(), &KDecoration2::DecorationSettings::spacingChanged, this, &Decoration::updateButtonsGeometryDelayed);
        connect(s.data(), &KDecoration2::DecorationSettings::decorationButtonsLeftChanged, this, &Decoration::updateButtonsGeometryDelayed);
        connect(s.data(), &KDecoration2::DecorationSettings::decorationButtonsRightChanged, this, &Decoration::updateButtonsGeometryDelayed);

        // full reconfiguration
        connect(s.data(), &KDecoration2::DecorationSettings::reconfigured, this, &Decoration::reconfigure);
        connect(s.data(), &KDecoration2::DecorationSettings::reconfigured, SettingsProvider::self(), &SettingsProvider::reconfigure, Qt::UniqueConnection );
        connect(s.data(), &KDecoration2::DecorationSettings::reconfigured, this, &Decoration::updateButtonsGeometryDelayed);

        connect(c, &KDecoration2::DecoratedClient::adjacentScreenEdgesChanged, this, &Decoration::recalculateBorders);
        connect(c, &KDecoration2::DecoratedClient::maximizedHorizontallyChanged, this, &Decoration::recalculateBorders);
        connect(c, &KDecoration2::DecoratedClient::maximizedVerticallyChanged, this, &Decoration::recalculateBorders);
        connect(c, &KDecoration2::DecoratedClient::shadedChanged, this, &Decoration::recalculateBorders);
        connect(c, &KDecoration2::DecoratedClient::captionChanged, this,
            [this]()
            {
                // update the caption area
                update(titleBar());
            }
        );

        connect(c, &KDecoration2::DecoratedClient::activeChanged, this, &Decoration::updateAnimationState);
        connect(c, &KDecoration2::DecoratedClient::widthChanged, this, &Decoration::updateTitleBar);
        connect(c, &KDecoration2::DecoratedClient::maximizedChanged, this, &Decoration::updateTitleBar);
        connect(c, &KDecoration2::DecoratedClient::maximizedChanged, this, &Decoration::setOpaque);

        connect(c, &KDecoration2::DecoratedClient::widthChanged, this, &Decoration::updateButtonsGeometry);
        connect(c, &KDecoration2::DecoratedClient::maximizedChanged, this, &Decoration::updateButtonsGeometry);
        connect(c, &KDecoration2::DecoratedClient::adjacentScreenEdgesChanged, this, &Decoration::updateButtonsGeometry);
        connect(c, &KDecoration2::DecoratedClient::shadedChanged, this, &Decoration::updateButtonsGeometry);

        createButtons();
        createShadow();
    }

    //________________________________________________________________
    void Decoration::updateTitleBar()
    {
        auto s = settings();
        auto c = client().toStrongRef().data();
        const bool maximized = isMaximized();
        const int width =  maximized ? c->width() : c->width() - 2*s->largeSpacing()*Metrics::TitleBar_SideMargin;
        const int height = maximized ? borderTop() : borderTop() - s->smallSpacing()*Metrics::TitleBar_TopMargin;
        const int x = maximized ? 0 : s->largeSpacing()*Metrics::TitleBar_SideMargin;
        const int y = maximized ? 0 : s->smallSpacing()*Metrics::TitleBar_TopMargin;
        setTitleBar(QRect(x, y, width, height));
    }

    //________________________________________________________________
    void Decoration::updateAnimationState()
    {
        if( m_internalSettings->animationsEnabled() )
        {

            const auto clientPtr = client().toStrongRef();
            m_animation->setDirection( (!clientPtr.isNull() && clientPtr.data()->isActive()) ? QAbstractAnimation::Forward : QAbstractAnimation::Backward );
            if( m_animation->state() != QAbstractAnimation::Running ) m_animation->start();

        } else {

            update();

        }
    }

    //________________________________________________________________
    int Decoration::borderSize() const
    {
        const int baseSize = settings()->smallSpacing();
        if( m_internalSettings && (m_internalSettings->mask() & BorderSize ) )
        {
            switch (m_internalSettings->borderSize()) {
                case InternalSettings::BorderNone: return 0;
                case InternalSettings::BorderNoSides: return 0;
                default:
                case InternalSettings::BorderTiny: return 1;
                case InternalSettings::BorderNormal: return baseSize*2;
                case InternalSettings::BorderLarge: return baseSize*3;
                case InternalSettings::BorderVeryLarge: return baseSize*4;
                case InternalSettings::BorderHuge: return baseSize*5;
                case InternalSettings::BorderVeryHuge: return baseSize*6;
                case InternalSettings::BorderOversized: return baseSize*10;
            }

        } else {

            switch (settings()->borderSize()) {
                case KDecoration2::BorderSize::None: return 0;
                case KDecoration2::BorderSize::NoSides: return 0;
                default:
                case KDecoration2::BorderSize::Tiny: return 1;
                case KDecoration2::BorderSize::Normal: return baseSize*2;
                case KDecoration2::BorderSize::Large: return baseSize*3;
                case KDecoration2::BorderSize::VeryLarge: return baseSize*4;
                case KDecoration2::BorderSize::Huge: return baseSize*5;
                case KDecoration2::BorderSize::VeryHuge: return baseSize*6;
                case KDecoration2::BorderSize::Oversized: return baseSize*10;

            }

        }
    }

    //________________________________________________________________
    void Decoration::reconfigure()
    {

        m_internalSettings = SettingsProvider::self()->internalSettings( this );

        // animation
        m_animation->setDuration( m_internalSettings->animationsDuration() );

        // borders
        recalculateBorders();

        // shadow
        createShadow();

    }

    //________________________________________________________________
    void Decoration::recalculateBorders()
    {
        auto c = client().toStrongRef();
        if (!c.isNull()) {
            auto s = settings();

            // left, right and bottom borders
            const int left   = isLeftEdge() ? 0 : borderSize();
            const int right  = isRightEdge() ? 0 : borderSize();
            const int bottom = (c->isShaded() || isBottomEdge()) ? 0 : borderSize();

            int top = 0;
            if( hideTitleBar() ) top = bottom;
            else {

                QFontMetrics fm(s->font());
                top += qMax(fm.height(), buttonHeight() );

                // padding below
                // extra pixel is used for the active window outline
                const int baseSize = s->smallSpacing();
                top += baseSize*Metrics::TitleBar_BottomMargin + 1;

                // padding above
                top += baseSize*TitleBar_TopMargin;

            }

            setBorders(QMargins(left, top, right, bottom));

            // extended resize borders
            const int extSize = s->largeSpacing();
            int extTop = isTopEdge() ? 0 : extSize;
            int extLeft = isLeftEdge() ? 0 : extSize;
            int extRight = isRightEdge() ? 0 : extSize;
            int extBottom = isBottomEdge() ? 0 : extSize;

            setResizeOnlyBorders(QMargins(extLeft, extTop, extRight, extBottom));

        }


    }

    //________________________________________________________________
    void Decoration::createButtons()
    {
        m_leftButtons = new KDecoration2::DecorationButtonGroup(KDecoration2::DecorationButtonGroup::Position::Left, this, &Button::create);
        m_rightButtons = new KDecoration2::DecorationButtonGroup(KDecoration2::DecorationButtonGroup::Position::Right, this, &Button::create);
        updateButtonsGeometry();
    }

    //________________________________________________________________
    void Decoration::updateButtonsGeometryDelayed()
    { QTimer::singleShot( 0, this, &Decoration::updateButtonsGeometry ); }

    //________________________________________________________________
    void Decoration::updateButtonsGeometry()
    {
        const auto s = settings();

        // adjust button position
        const int bHeight = captionHeight() + (isTopEdge() ? s->smallSpacing()*Metrics::TitleBar_TopMargin:0);
        const int bWidth = buttonHeight();
        const int verticalOffset = (isTopEdge() ? s->smallSpacing()*Metrics::TitleBar_TopMargin:0) + (captionHeight()-buttonHeight())/2;
        foreach( const QPointer<KDecoration2::DecorationButton>& button, m_leftButtons->buttons() + m_rightButtons->buttons() )
        {
            button.data()->setGeometry( QRectF( QPoint( 0, 0 ), QSizeF( bWidth, bHeight ) ) );
            static_cast<Button*>( button.data() )->setOffset( QPointF( 0, verticalOffset ) );
            static_cast<Button*>( button.data() )->setIconSize( QSize( bWidth, bWidth ) );
        }

        // left buttons
        if( !m_leftButtons->buttons().isEmpty() )
        {

            // spacing
            m_leftButtons->setSpacing(s->smallSpacing()*Metrics::TitleBar_ButtonSpacing);

            // padding
            const int vPadding = isTopEdge() ? 0 : s->smallSpacing()*Metrics::TitleBar_TopMargin;
            const int hPadding = s->smallSpacing()*Metrics::TitleBar_SideMargin;
            if( isLeftEdge() )
            {
                // add offsets on the side buttons, to preserve padding, but satisfy Fitts law
                auto button = static_cast<Button*>( m_leftButtons->buttons().front().data() );
                button->setGeometry( QRectF( QPoint( 0, 0 ), QSizeF( bWidth + hPadding, bHeight ) ) );
                button->setFlag( Button::FlagFirstInList );
                button->setHorizontalOffset( hPadding );

                m_leftButtons->setPos(QPointF(0, vPadding));

            } else m_leftButtons->setPos(QPointF(hPadding + borderLeft(), vPadding));

        }

        // right buttons
        if( !m_rightButtons->buttons().isEmpty() )
        {

            // spacing
            m_rightButtons->setSpacing(s->smallSpacing()*Metrics::TitleBar_ButtonSpacing);

            // padding
            const int vPadding = isTopEdge() ? 0 : s->smallSpacing()*Metrics::TitleBar_TopMargin;
            const int hPadding = s->smallSpacing()*Metrics::TitleBar_SideMargin;
            if( isRightEdge() )
            {

                auto button = static_cast<Button*>( m_rightButtons->buttons().back().data() );
                button->setGeometry( QRectF( QPoint( 0, 0 ), QSizeF( bWidth + hPadding, bHeight ) ) );
                button->setFlag( Button::FlagLastInList );

                m_rightButtons->setPos(QPointF(size().width() - m_rightButtons->geometry().width(), vPadding));

            } else m_rightButtons->setPos(QPointF(size().width() - m_rightButtons->geometry().width() - hPadding - borderRight(), vPadding));

        }

        update();

    }

    //________________________________________________________________
    void Decoration::paint(QPainter *painter, const QRect &repaintRegion)
    {
        // TODO: optimize based on repaintRegion
        auto c = client().toStrongRef().data();
        auto s = settings();

        // paint background
        if( !c->isShaded() )
        {
            painter->fillRect(rect(), Qt::transparent);
            painter->save();
            painter->setPen(Qt::NoPen);

            if ( !s->isAlphaChannelSupported() ) {
                painter->setRenderHint(QPainter::Antialiasing);
                painter->setBrush( titleBarColor() );

                // clip away the top part
                if( !hideTitleBar() ) painter->setClipRect(0, borderTop(), size().width(), size().height() - borderTop(), Qt::IntersectClip);

                painter->drawRect( rect() );

            } else {
                QRect rectAdjusted = rect();

                if ( !hasNoBorders() ) {
                    painter->setBrush( outlineColor() );

                    // clip away the top part
                    if( !hideTitleBar() ) painter->setClipRect(0, borderTop(), size().width(), size().height() - borderTop(), Qt::IntersectClip);
                    painter->drawRect( rectAdjusted );

                    rectAdjusted = rectAdjusted.adjusted(1, 1, -1, -1);
                }


                painter->setRenderHint(QPainter::Antialiasing);
                painter->setBrush( titleBarColor() );

                // clip away the top part
                if( !hideTitleBar() ) painter->setClipRect(0, borderTop(), size().width(), size().height() - borderTop(), Qt::IntersectClip);

                painter->drawRect(rectAdjusted );

            }

            painter->restore();
        }

        if( !hideTitleBar() ) paintTitleBar(painter, repaintRegion);

        if( hasBorders() && !s->isAlphaChannelSupported() )
        {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, false);
            painter->setBrush( Qt::NoBrush );
            painter->setPen( outlineColor() );
            painter->drawRect( rect().adjusted( 0, 0, -1, -1 ) );
            painter->restore();
        }

    }

    //________________________________________________________________
    void Decoration::paintTitleBar(QPainter *painter, const QRect &repaintRegion)
    {
        const auto clientPtr = client().toStrongRef();
        const bool noBorders = hasNoSideBorders() || hasNoBorders();
        const QRect titleRect(QPoint(0, 0), QSize(size().width(), borderTop()));

        if ( !titleRect.intersects(repaintRegion) || clientPtr.isNull() ) return;

        const auto c = clientPtr.data();

        painter->save();

        painter->setPen(Qt::NoPen);
        painter->setBrush( titleBarColor() );

        auto s = settings();
        if( isMaximized() )
        {
            painter->drawRect(titleRect);
            paintTitleBarShading(painter, titleRect, false);

        } else if( !s->isAlphaChannelSupported() ) {
            painter->setClipRect(titleRect);
            painter->drawRect(titleRect.adjusted(0, 0, 0, c->isShaded() ? 0 : 1));
            paintTitleBarShading(painter, noBorders ? titleRect : titleRect.adjusted(1, 1, -1, 0), false);

        } else if( c->isShaded() ) {
            QRect titleRectAdjusted = noBorders ? titleRect : titleRect.adjusted(1, 1, -1, -1);

            if ( !noBorders ) {
                painter->setBrush( outlineColor() );
                painter->drawRoundedRect(titleRect, Metrics::Frame_FrameRadius, Metrics::Frame_FrameRadius);
                titleRectAdjusted = titleRect.adjusted(1, 1, -1, -1);
            }

            painter->setBrush( titleBarColor() );
            painter->drawRoundedRect(titleRectAdjusted, Metrics::Frame_FrameRadius, Metrics::Frame_FrameRadius);
            smoothenTitleBarCorners(painter, titleRectAdjusted, true);
            paintTitleBarShading(painter, titleRectAdjusted, true);

        } else {
            QRect titleRectAdjusted = noBorders ? titleRect.adjusted(0, 0, 0, Metrics::Frame_FrameRadius) : titleRect.adjusted(1, 1, -1, Metrics::Frame_FrameRadius-1);

            if ( !noBorders ) {
                painter->setClipRect(titleRect, Qt::IntersectClip);
                painter->setBrush( outlineColor() );
                painter->drawRoundedRect(titleRect.adjusted(0, 0, 0, Metrics::Frame_FrameRadius), Metrics::Frame_FrameRadius+1, Metrics::Frame_FrameRadius+1);
                titleRectAdjusted = titleRect.adjusted(1, 1, -1, Metrics::Frame_FrameRadius);
            }

            painter->setClipRect(titleRect, Qt::IntersectClip);
            painter->setBrush( titleBarColor() );
            painter->drawRoundedRect(titleRectAdjusted, Metrics::Frame_FrameRadius, Metrics::Frame_FrameRadius);
            smoothenTitleBarCorners(painter, titleRectAdjusted, false);
            paintTitleBarShading(painter, titleRectAdjusted, true);

        }

        painter->restore();

        // draw caption
        painter->setFont(s->font());
        painter->setPen( fontColor() );
        const auto cR = captionRect();
        const QString caption = painter->fontMetrics().elidedText(c->caption(), Qt::ElideMiddle, cR.first.width());
        painter->drawText(cR.first, cR.second | Qt::TextSingleLine, caption);

        // draw all buttons
        m_leftButtons->paint(painter, repaintRegion);
        m_rightButtons->paint(painter, repaintRegion);
    }

    //________________________________________________________________
    void Decoration::paintTitleBarShading(QPainter *painter, const QRect &titleRect, const bool rounded) {

        painter->setPen( highlightColor() );
        painter->setBrush( Qt::NoBrush );
        painter->setRenderHint( QPainter::Antialiasing, false );

        if (rounded) {
            const QRect adjustedRect = titleRect.adjusted(-1, 0, 0, 0);
            painter->setClipRect(adjustedRect.adjusted(0, 0, 0, 2 - adjustedRect.height()), Qt::IntersectClip);
            painter->drawRoundedRect(adjustedRect, Metrics::Frame_FrameRadius, Metrics::Frame_FrameRadius);
        } else {
            painter->drawLine( titleRect.topLeft(), titleRect.topRight() );
        }

    }


    //________________________________________________________________
    void Decoration::smoothenTitleBarCorners(QPainter *painter, const QRect &titleRect, bool bottom) {

        QPen pen { titleBarColor(), 1.5 };
        painter->setPen( pen );
        painter->setBrush( Qt::NoBrush );
        painter->setRenderHint( QPainter::Antialiasing, false );
        painter->setClipRect(titleRect, Qt::ReplaceClip);

        QRect arcSize = titleRect.adjusted(0, 0, 7 - titleRect.width(), 7 - titleRect.height());

        painter->drawArc(arcSize, 90*16, 90*16);
        painter->drawArc(arcSize.translated(titleRect.width() - arcSize.width(), 0), 0*16, 90*16);
        if (bottom) {
            painter->drawArc(arcSize.translated(0, titleRect.height() - arcSize.height()), 180*16, 90*16);
            painter->drawArc(arcSize.translated(titleRect.width() - arcSize.width(), titleRect.height() - arcSize.height()), 270*16, 90*16);
        }
    }

    //________________________________________________________________
    int Decoration::buttonHeight() const
    {
        const int baseSize = settings()->gridUnit();
        switch( m_internalSettings->buttonSize() )
        {
            case InternalSettings::ButtonTiny: return baseSize;
            case InternalSettings::ButtonSmall: return baseSize*1.5;
            default:
            case InternalSettings::ButtonDefault: return baseSize*2;
            case InternalSettings::ButtonLarge: return baseSize*2.5;
            case InternalSettings::ButtonVeryLarge: return baseSize*3.5;
        }

    }

    //________________________________________________________________
    int Decoration::captionHeight() const
    { return hideTitleBar() ? borderTop() : borderTop() - settings()->smallSpacing()*(Metrics::TitleBar_BottomMargin + Metrics::TitleBar_TopMargin ) - 1; }

    //________________________________________________________________
    QPair<QRect,Qt::Alignment> Decoration::captionRect() const
    {
        if( hideTitleBar() ) return qMakePair( QRect(), Qt::AlignCenter );
        else {

            auto c = client().toStrongRef().data();
            const int leftOffset = m_leftButtons->buttons().isEmpty() ?
                Metrics::TitleBar_SideMargin*settings()->smallSpacing():
                m_leftButtons->geometry().x() + m_leftButtons->geometry().width() + Metrics::TitleBar_SideMargin*settings()->smallSpacing();

            const int rightOffset = m_rightButtons->buttons().isEmpty() ?
                Metrics::TitleBar_SideMargin*settings()->smallSpacing() :
                size().width() - m_rightButtons->geometry().x() + Metrics::TitleBar_SideMargin*settings()->smallSpacing();

            const int yOffset = settings()->smallSpacing()*Metrics::TitleBar_TopMargin;
            const QRect maxRect( leftOffset, yOffset, size().width() - leftOffset - rightOffset, captionHeight() );

            switch( m_internalSettings->titleAlignment() )
            {
                case InternalSettings::AlignLeft:
                return qMakePair( maxRect, Qt::AlignVCenter|Qt::AlignLeft );

                case InternalSettings::AlignRight:
                return qMakePair( maxRect, Qt::AlignVCenter|Qt::AlignRight );

                case InternalSettings::AlignCenter:
                return qMakePair( maxRect, Qt::AlignCenter );

                default:
                case InternalSettings::AlignCenterFullWidth:
                {

                    // full caption rect
                    const QRect fullRect = QRect( 0, yOffset, size().width(), captionHeight() );
                    QRect boundingRect( settings()->fontMetrics().boundingRect( c->caption()).toRect() );

                    // text bounding rect
                    boundingRect.setTop( yOffset );
                    boundingRect.setHeight( captionHeight() );
                    boundingRect.moveLeft( ( size().width() - boundingRect.width() )/2 );

                    if( boundingRect.left() < leftOffset ) return qMakePair( maxRect, Qt::AlignVCenter|Qt::AlignLeft );
                    else if( boundingRect.right() > size().width() - rightOffset ) return qMakePair( maxRect, Qt::AlignVCenter|Qt::AlignRight );
                    else return qMakePair(fullRect, Qt::AlignCenter);

                }

            }

        }

    }

    //________________________________________________________________
    void Decoration::createShadow()
    {
        if (!g_sShadow
                ||g_shadowSizeEnum != m_internalSettings->shadowSize()
                || g_shadowStrength != m_internalSettings->shadowStrength()
                || g_shadowColor != m_internalSettings->shadowColor())
        {
            g_shadowSizeEnum = m_internalSettings->shadowSize();
            g_shadowStrength = m_internalSettings->shadowStrength();
            g_shadowColor = m_internalSettings->shadowColor();

            const CompositeShadowParams params = lookupShadowParams(g_shadowSizeEnum);
            if (params.isNone()) {
                g_sShadow.clear();
                setShadow(g_sShadow);
                return;
            }

            auto withOpacity = [](const QColor &color, qreal opacity) -> QColor {
                QColor c(color);
                c.setAlphaF(opacity);
                return c;
            };

            const QSize boxSize = Breeze::BoxShadowRenderer::calculateMinimumBoxSize(params.shadow1.radius)
                .expandedTo(Breeze::BoxShadowRenderer::calculateMinimumBoxSize(params.shadow2.radius));

            Breeze::BoxShadowRenderer shadowRenderer;
            shadowRenderer.setBorderRadius(Metrics::Frame_FrameRadius + 0.5);
            shadowRenderer.setBoxSize(boxSize);
            shadowRenderer.setDevicePixelRatio(1.0); // TODO: Create HiDPI shadows?

            const qreal strength = static_cast<qreal>(g_shadowStrength) / 255.0;
            shadowRenderer.addShadow(params.shadow1.offset, params.shadow1.radius,
                withOpacity(g_shadowColor, params.shadow1.opacity * strength));
            shadowRenderer.addShadow(params.shadow2.offset, params.shadow2.radius,
                withOpacity(g_shadowColor, params.shadow2.opacity * strength));

            QImage shadowTexture = shadowRenderer.render();

            QPainter painter(&shadowTexture);
            painter.setRenderHint(QPainter::Antialiasing);

            const QRect outerRect = shadowTexture.rect();

            QRect boxRect(QPoint(0, 0), boxSize);
            boxRect.moveCenter(outerRect.center());

            // Mask out inner rect.
            const QMargins padding = QMargins(
                boxRect.left() - outerRect.left() - Metrics::Shadow_Overlap - params.offset.x(),
                boxRect.top() - outerRect.top() - Metrics::Shadow_Overlap - params.offset.y(),
                outerRect.right() - boxRect.right() - Metrics::Shadow_Overlap + params.offset.x(),
                outerRect.bottom() - boxRect.bottom() - Metrics::Shadow_Overlap + params.offset.y());
            const QRect innerRect = outerRect - padding;

            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::black);
            painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
            painter.drawRoundedRect(
                innerRect,
                Metrics::Frame_FrameRadius + 0.5,
                Metrics::Frame_FrameRadius + 0.5);

            // Draw outline.
            painter.setPen(withOpacity(g_shadowColor, 0.2 * strength));
            painter.setBrush(Qt::NoBrush);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.drawRoundedRect(
                innerRect,
                Metrics::Frame_FrameRadius - 0.5,
                Metrics::Frame_FrameRadius - 0.5);

            painter.end();

            g_sShadow = QSharedPointer<KDecoration2::DecorationShadow>::create();
            g_sShadow->setPadding(padding);
            g_sShadow->setInnerShadowRect(QRect(outerRect.center(), QSize(1, 1)));
            g_sShadow->setShadow(shadowTexture);
        }

        setShadow(g_sShadow);
    }

} // namespace


#include "arcdecoration.moc"
