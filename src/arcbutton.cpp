/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 * Copyright 2014  Hugo Pereira Da Costa <hugo.pereira@free.fr>
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
#include "arcbutton.h"

#include <KDecoration2/DecoratedClient>
#include <KColorUtils>

#include <QPainter>
#include <QVariantAnimation>

namespace Arc
{
    using KDecoration2::DecorationButtonType;

    static const QColor LIGHT_ICON_BG { "#90949E" };
    static const QColor LIGHT_ICON_UNFOCUSED_BG { "#B6B8C0" };
    static const QColor LIGHT_ICON_HOVER_BG { "#7A7F8B" };
    static const QColor LIGHT_ICON_ACTIVE_BG { "#FFFFFF" };

    static const QColor LIGHT_BUTTON_HOVER_BG { "#fdfdfd" };
    static const QColor LIGHT_BUTTON_ACTIVE_BG { "#5294e2" };
    static const QColor LIGHT_BUTTON_HOVER_BORDER { "#D1D3DA" };
    static const QColor LIGHT_BUTTON_SELECTED_BG { "#5294e2" };

    static const QColor LIGHT_BUTTON_CLOSE_BG { "#f46067" };
    static const QColor LIGHT_BUTTON_CLOSE_HOVER_BG { "#f68086" };
    static const QColor LIGHT_BUTTON_CLOSE_ACTIVE_BG { "#f13039" };

    static const QColor DARK_ICON_BG { "#90939B" };
    static const QColor DARK_ICON_UNFOCUSED_BG { "#666A74" };
    static const QColor DARK_ICON_HOVER_BG { "#C4C7CC" };
    static const QColor DARK_ICON_ACTIVE_BG { "#FFFFFF" };

    static const QColor DARK_BUTTON_HOVER_BG { "#454C5C" };
    static const QColor DARK_BUTTON_ACTIVE_BG { "#5294e2" };
    static const QColor DARK_BUTTON_HOVER_BORDER { "#262932" };
    static const QColor DARK_BUTTON_SELECTED_BG { "#5294e2" };

    static const QColor DARK_BUTTON_CLOSE_BG { "#cc575d" };
    static const QColor DARK_BUTTON_CLOSE_HOVER_BG { "#d7787d" };
    static const QColor DARK_BUTTON_CLOSE_ACTIVE_BG { "#be3841" };

    //__________________________________________________________________
    Button::Button(DecorationButtonType type, Decoration* decoration, QObject* parent)
        : DecorationButton(type, decoration, parent)
        , m_animation( new QVariantAnimation( this ) )
    {

        // setup animation
        // It is important start and end value are of the same type, hence 0.0 and not just 0
        m_animation->setStartValue( 0.0 );
        m_animation->setEndValue( 1.0 );
        m_animation->setEasingCurve( QEasingCurve::InOutQuad );
        connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            setOpacity(value.toReal());
        });

        // setup default geometry
        const int height = decoration->buttonHeight();
        setGeometry(QRect(0, 0, height, height));
        setIconSize(QSize( height, height ));

        // connections
        connect(decoration->client().toStrongRef().data(), SIGNAL(iconChanged(QIcon)), this, SLOT(update()));
        connect(decoration->settings().data(), &KDecoration2::DecorationSettings::reconfigured, this, &Button::reconfigure);
        connect( this, &KDecoration2::DecorationButton::hoveredChanged, this, &Button::updateAnimationState );

        reconfigure();

    }

    //__________________________________________________________________
    Button::Button(QObject *parent, const QVariantList &args)
        : Button(args.at(0).value<DecorationButtonType>(), args.at(1).value<Decoration*>(), parent)
    {
        m_flag = FlagStandalone;
        //! icon size must return to !valid because it was altered from the default constructor,
        //! in Standalone mode the button is not using the decoration metrics but its geometry
        m_iconSize = QSize(-1, -1);
    }

    //__________________________________________________________________
    Button *Button::create(DecorationButtonType type, KDecoration2::Decoration *decoration, QObject *parent)
    {
        if (auto d = qobject_cast<Decoration*>(decoration))
        {
            Button *b = new Button(type, d, parent);

            auto strongPtr = d->client().toStrongRef();
            if (!strongPtr.isNull()) {
                switch( type )
                {

                    case DecorationButtonType::Close:
                    b->setVisible( strongPtr.data()->isCloseable() );
                    QObject::connect(strongPtr.data(), &KDecoration2::DecoratedClient::closeableChanged, b, &Arc::Button::setVisible );
                    break;

                    case DecorationButtonType::Maximize:
                    b->setVisible( strongPtr.data()->isMaximizeable() );
                    QObject::connect(strongPtr.data(), &KDecoration2::DecoratedClient::maximizeableChanged, b, &Arc::Button::setVisible );
                    break;

                    case DecorationButtonType::Minimize:
                    b->setVisible( strongPtr.data()->isMinimizeable() );
                    QObject::connect(strongPtr.data(), &KDecoration2::DecoratedClient::minimizeableChanged, b, &Arc::Button::setVisible );
                    break;

                    case DecorationButtonType::ContextHelp:
                    b->setVisible( strongPtr.data()->providesContextHelp() );
                    QObject::connect(strongPtr.data(), &KDecoration2::DecoratedClient::providesContextHelpChanged, b, &Arc::Button::setVisible );
                    break;

                    case DecorationButtonType::Shade:
                    b->setVisible( strongPtr.data()->isShadeable() );
                    QObject::connect(strongPtr.data(), &KDecoration2::DecoratedClient::shadeableChanged, b, &Arc::Button::setVisible );
                    break;

                    case DecorationButtonType::Menu:
                    QObject::connect(strongPtr.data(), &KDecoration2::DecoratedClient::iconChanged, b, [b]() { b->update(); });
                    break;

                    default: break;

                }
            }

            return b;
        }

        return nullptr;

    }

    bool Button::isCheckedCustom() const {
        if (type() == DecorationButtonType::Maximize) {
            return false;
        } else {
            return isChecked();
        }
    }

    //__________________________________________________________________
    void Button::paint(QPainter *painter, const QRect &repaintRegion)
    {
        Q_UNUSED(repaintRegion)

        if (!decoration()) return;

        painter->save();

        // translate from offset
        if( m_flag == FlagFirstInList ) painter->translate( m_offset );
        else painter->translate( 0, m_offset.y() );

        if( !m_iconSize.isValid() ) m_iconSize = geometry().size().toSize();

        // menu button
        if (type() == DecorationButtonType::Menu)
        {

            const QRectF iconRect( geometry().topLeft(), m_iconSize );
            decoration()->client().toStrongRef().data()->icon().paint(painter, iconRect.toRect());


        } else {

            drawIcon( painter );

        }

        painter->restore();

    }

    //__________________________________________________________________
    void Button::drawIcon( QPainter *painter ) const
    {

        painter->setRenderHints( QPainter::Antialiasing );

        /*
        scale painter so that its window matches QRect( -1, -1, 20, 20 )
        this makes all further rendering and scaling simpler
        all further rendering is preformed inside QRect( 0, 0, 18, 18 )
        */
        painter->translate( geometry().topLeft() );

        const qreal width( m_iconSize.width() );
        painter->scale( width/20, width/20 );
        painter->translate( 1, 1 );

        // render background
        const QColor backgroundColor( this->backgroundColor() );
        if( backgroundColor.isValid() )
        {
            painter->setBrush( backgroundColor );
            if ( type() != DecorationButtonType::Close && !isPressed() && !isCheckedCustom() &&
            (isHovered() || (m_animation->state() == QAbstractAnimation::Running ))) {
                QPen pen( m_buttonHoverBorder );
                pen.setWidthF(1.25);
                painter->setPen( pen );
                painter->drawEllipse( QRectF( 2.5, 2.5, 13, 13 ) );

            } else {
                painter->setPen( Qt::NoPen );
                painter->drawEllipse( QRectF( 2, 2, 14, 14 ) );
            }
        }

        // render mark
        const QColor foregroundColor( this->foregroundColor() );
        if( foregroundColor.isValid() )
        {

            // setup painter
            QPen pen( foregroundColor );
            pen.setCapStyle( Qt::RoundCap );
            pen.setJoinStyle( Qt::MiterJoin );
            pen.setWidthF( PenWidth::Symbol*qMax((qreal)1.0, 20/width ) );

            painter->setBrush( foregroundColor );
            painter->setPen( Qt::NoPen );

            switch( type() )
            {

                case DecorationButtonType::Close:
                {
                    painter->setBrush( Qt::NoBrush );
                    pen.setWidthF( PenWidth::Symbol*qMax((qreal)1.75, 35/width ) );
                    painter->setPen( pen );
                    painter->drawLine( QPointF( 7, 7 ), QPointF( 11, 11 ) );
                    painter->drawLine( 11, 7, 7, 11 );
                    break;
                }

                case DecorationButtonType::Maximize:
                {
                    if ( isChecked() ) {

                        painter->drawPolygon( QVector<QPointF>{
                            QPointF( 9, 9.8 ),
                            QPointF( 9, 13.2 ),
                            QPointF( 4.8, 9 ),
                            QPointF( 8.2, 9 )} );

                        painter->drawPolygon( QVector<QPointF>{
                            QPointF( 9, 8.2 ),
                            QPointF( 9, 4.8 ),
                            QPointF( 13.2, 9 ),
                            QPointF( 9.8, 9 )} );
                    } else {

                        painter->drawPolygon( QVector<QPointF>{
                            QPointF( 6, 11.4 ),
                            QPointF( 6, 7.6 ),
                            QPointF( 10.4, 12 ),
                            QPointF( 6.6, 12 )} );

                        painter->drawPolygon( QVector<QPointF>{
                            QPointF( 12, 6.6 ),
                            QPointF( 12, 10.4 ),
                            QPointF( 7.6, 6 ),
                            QPointF( 11.4, 6 )} );
                    }

                    break;
                }

                case DecorationButtonType::Minimize:
                {
                    painter->drawRect( QRectF( 6, 8, 6, 2 ) );
                    break;
                }

                case DecorationButtonType::OnAllDesktops:
                {

                    auto d = qobject_cast<Decoration*>(decoration());
                    if (d && d->internalSettings()->auroraeIcons()) {

                        painter->drawEllipse( QRectF( 6, 6, 6, 6 ) );

                    } else {

                        painter->drawPolygon( QVector<QPointF>{
                            QPointF( 10, 6.5 ),
                            QPointF( 10, 4 ),
                            QPointF( 14, 8 ),
                            QPointF( 11.5, 8 ),
                            QPointF( 10, 9.5 ),
                            QPointF( 10, 12 ),
                            QPointF( 8, 11 ),
                            QPointF( 7, 10 ),
                            QPointF( 6, 8 ),
                            QPointF( 8.5, 8 )
                        });

                        painter->setPen( pen );
                        painter->drawLine( QPointF( 11, 7 ), QPointF( 5.5, 12.5 ) );
                    }
                    break;
                }

                case DecorationButtonType::Shade:
                {
                    auto d = qobject_cast<Decoration*>(decoration());
                    if (d && d->internalSettings()->auroraeIcons()) {

                        painter->drawPolygon( QVector<QPointF>{
                            QPointF( 6, 9 ),
                            QPointF( 9, 6 ),
                            QPointF( 12, 9 )} );
                        painter->drawRect( QRectF( 8, 9, 2, 2.5 ) );

                    } else {

                        painter->drawRect( QRectF( 6, 6, 6, 2 ) );
                        painter->drawPolygon( QVector<QPointF>{
                            QPointF( 5.5, 12 ),
                            QPointF( 9, 8.5 ),
                            QPointF( 12.5, 12 )} );
                    }
                    break;

                }

                case DecorationButtonType::KeepBelow:
                {
                    auto d = qobject_cast<Decoration*>(decoration());
                    if (d && d->internalSettings()->auroraeIcons()) {
                        painter->drawPolygon( QVector<QPointF>{
                            QPointF( 6, 7 ),
                            QPointF( 9, 12 ),
                            QPointF( 12, 7 )} );

                    } else {

                        // drawing each dot separately seems to be the best way of ensuring
                        // that scaling works properly
                        painter->drawRect( QRectF( 8, 5, 1, 1) );
                        painter->drawRect( QRectF( 10, 5, 1, 1) );
                        painter->drawRect( QRectF( 12, 5, 1, 1) );
                        painter->drawRect( QRectF( 12, 7, 1, 1) );
                        painter->drawRect( QRectF( 12, 9, 1, 1) );
                        painter->drawRect( QRectF( 10, 9, 1, 1) );
                        painter->drawRect( QRectF( 8, 9, 1, 1) );
                        painter->drawRect( QRectF( 8, 7, 1, 1) );

                        painter->drawRect( QRectF( 5, 8, 1, 4.5) );
                        painter->drawRect( QRectF( 5, 12, 5, 1) );

                    }
                    break;

                }

                case DecorationButtonType::KeepAbove:
                {
                    auto d = qobject_cast<Decoration*>(decoration());
                    if (d && d->internalSettings()->auroraeIcons()) {
                        painter->drawPolygon( QVector<QPointF>{
                            QPointF( 6, 11 ),
                            QPointF( 9, 6 ),
                            QPointF( 12, 11 )} );

                    } else {
                        painter->drawRect( QRectF( 8, 5, 5, 5 ) );

                        // drawing each dot separately seems to be the best way of ensuring
                        // scaling works properly
                        painter->drawRect( QRectF( 5, 8, 1, 1) );
                        painter->drawRect( QRectF( 5, 10, 1, 1) );
                        painter->drawRect( QRectF( 5, 12, 1, 1) );
                        painter->drawRect( QRectF( 7, 12, 1, 1) );
                        painter->drawRect( QRectF( 9, 12, 1, 1) );

                    }
                    break;
                }


                case DecorationButtonType::ApplicationMenu:
                {
                    painter->setPen( pen );
                    painter->setBrush( Qt::NoBrush );
                    painter->drawRect( QRectF( 5.5, 5.5, 7, 1 ) );
                    painter->drawRect( QRectF( 5.5, 8.5, 7, 1 ) );
                    painter->drawRect( QRectF( 5.5, 11.5, 7, 1 ) );
                    break;
                }

                case DecorationButtonType::ContextHelp:
                {
                    painter->setBrush( Qt::NoBrush );
                    QPen pen { foregroundColor };
                    pen.setCapStyle( Qt::FlatCap );
                    pen.setJoinStyle( Qt::MiterJoin );
                    pen.setWidthF( PenWidth::Symbol*qMax((qreal)1.5, 30/width ) );
                    painter->setPen( pen );

                    QPainterPath path;
                    path.moveTo( 6.5, 7.5 );
                    path.arcTo( QRectF( 6.5, 5, 5, 3 ), 180, -180 );
                    path.cubicTo( QPointF(11.5, 9), QPointF( 8, 8 ), QPointF( 9, 10.5 ) );
                    painter->drawPath( path );

                    painter->drawLine( QPointF( 9, 12 ), QPointF( 9, 14 ) );

                    break;
                }

                default: break;

            }

        }

    }

    //__________________________________________________________________
    QColor Button::foregroundColor() const
    {
        auto d = qobject_cast<Decoration*>( decoration() );
        auto clientPtr = d ? d->client().toStrongRef() : QSharedPointer<KDecoration2::DecoratedClient>();

        if ( clientPtr.isNull() ) {

            return QColor();

        }
        else if( (isPressed() || (isCheckedCustom())) && type() != DecorationButtonType::Close ) {

            return m_iconActiveBg;

        } else if( type() == DecorationButtonType::Close ) {

            return d->titleBarColor();

        } else if( m_animation->state() == QAbstractAnimation::Running ) {

            const QColor baseColor = clientPtr.data()->isActive() ? m_iconBg : m_iconUnfocusedBg;
            return KColorUtils::mix( baseColor, m_iconHoverBg, m_opacity );

        } else if( isHovered() ) {

            return m_iconHoverBg;

        } else {

            return (clientPtr.data()->isActive() ? m_iconBg : m_iconUnfocusedBg);

        }

    }

    //__________________________________________________________________
    QColor Button::backgroundColor() const
    {
        auto d = qobject_cast<Decoration*>( decoration() );
        auto clientPtr = d ? d->client().toStrongRef() : QSharedPointer<KDecoration2::DecoratedClient>();
        if( clientPtr.isNull() ) {

            return QColor();

        }

        auto c = clientPtr.data();
        if( isPressed() ) {
            if ( type() == DecorationButtonType::Close ) return m_buttonCloseActiveBg;
            else return m_buttonActiveBg;

        } else if( isCheckedCustom() ) {
            return m_buttonSelectedBg;

        } else if( m_animation->state() == QAbstractAnimation::Running ) {

            if( type() == DecorationButtonType::Close ) {

                const QColor baseColor { c->isActive() ? m_buttonCloseBg : m_iconUnfocusedBg };
                return KColorUtils::mix( baseColor, m_buttonCloseHoverBg, m_opacity );

            } else {
                QColor color = m_buttonHoverBg;
                color.setAlpha( color.alpha()*m_opacity );
                return color;

            }

        } else if( isHovered() ) {

            if( type() == DecorationButtonType::Close ) return m_buttonCloseHoverBg;
            else return m_buttonHoverBg;

        } else if( type() == DecorationButtonType::Close) {

            return ( c->isActive() ? m_buttonCloseBg : m_iconUnfocusedBg);

        } else {

            return QColor();

        }

    }

    //________________________________________________________________
    void Button::reconfigure()
    {

        // animation
        auto d = qobject_cast<Decoration*>(decoration());
        if( d ) {
            m_animation->setDuration( d->internalSettings()->animationsDuration() );
            if (static_cast<InternalSettings::EnumArcTheme>(d->internalSettings()->arcTheme()) == InternalSettings::ThemeDark) {
                m_iconBg = DARK_ICON_BG;
                m_iconUnfocusedBg = DARK_ICON_UNFOCUSED_BG;
                m_iconHoverBg = DARK_ICON_HOVER_BG;
                m_iconActiveBg = DARK_ICON_ACTIVE_BG;

                m_buttonHoverBg = DARK_BUTTON_HOVER_BG;
                m_buttonActiveBg = DARK_BUTTON_ACTIVE_BG;
                m_buttonHoverBorder = DARK_BUTTON_HOVER_BORDER;
                m_buttonSelectedBg = DARK_BUTTON_SELECTED_BG;

                m_buttonCloseBg = DARK_BUTTON_CLOSE_BG;
                m_buttonCloseHoverBg = DARK_BUTTON_CLOSE_HOVER_BG;
                m_buttonCloseActiveBg = DARK_BUTTON_CLOSE_ACTIVE_BG;
            } else {
                m_iconBg = LIGHT_ICON_BG;
                m_iconUnfocusedBg = LIGHT_ICON_UNFOCUSED_BG;
                m_iconHoverBg = LIGHT_ICON_HOVER_BG;
                m_iconActiveBg = LIGHT_ICON_ACTIVE_BG;

                m_buttonHoverBg = LIGHT_BUTTON_HOVER_BG;
                m_buttonActiveBg = LIGHT_BUTTON_ACTIVE_BG;
                m_buttonHoverBorder = LIGHT_BUTTON_HOVER_BORDER;
                m_buttonSelectedBg = LIGHT_BUTTON_SELECTED_BG;

                m_buttonCloseBg = LIGHT_BUTTON_CLOSE_BG;
                m_buttonCloseHoverBg = LIGHT_BUTTON_CLOSE_HOVER_BG;
                m_buttonCloseActiveBg = LIGHT_BUTTON_CLOSE_ACTIVE_BG;
            }
        }

    }

    //__________________________________________________________________
    void Button::updateAnimationState( bool hovered )
    {

        auto d = qobject_cast<Decoration*>(decoration());
        if( !(d && d->internalSettings()->animationsEnabled() ) ) return;

        m_animation->setDirection( hovered ? QAbstractAnimation::Forward : QAbstractAnimation::Backward );
        if( m_animation->state() != QAbstractAnimation::Running ) m_animation->start();

    }

} // namespace
