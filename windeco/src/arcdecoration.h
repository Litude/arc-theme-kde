#ifndef ARC_DECORATION_H
#define ARC_DECORATION_H

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

#include "arc.h"
#include "arcsettings.h"

#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>
#include <KDecoration2/DecorationSettings>

#include <QPalette>
#include <QVariant>

class QVariantAnimation;

namespace KDecoration2
{
    class DecorationButton;
    class DecorationButtonGroup;
}

namespace Arc
{
    class Decoration : public KDecoration2::Decoration
    {
        Q_OBJECT

        public:

        //* constructor
        explicit Decoration(QObject *parent = nullptr, const QVariantList &args = QVariantList());

        //* destructor
        virtual ~Decoration();

        //* paint
        void paint(QPainter *painter, const QRect &repaintRegion) override;

        //* internal settings
        InternalSettingsPtr internalSettings() const
        { return m_internalSettings; }

        //* caption height
        int captionHeight() const;

        //* button height
        int buttonHeight() const;

        //*@name active state change animation
        //@{
        void setOpacity( qreal );

        qreal opacity() const
        { return m_opacity; }

        //@}

        //*@name colors
        //@{
        QColor titleBarColor() const;
        QColor outlineColor() const;
        QColor highlightColor() const;
        QColor fontColor() const;
        //@}

        //*@name maximization modes
        //@{
        inline bool isMaximized() const;
        inline bool isMaximizedHorizontally() const;
        inline bool isMaximizedVertically() const;

        inline bool isLeftEdge() const;
        inline bool isRightEdge() const;
        inline bool isTopEdge() const;
        inline bool isBottomEdge() const;

        inline bool hideTitleBar() const;
        //@}

        public Q_SLOTS:
        void init() override;

        private Q_SLOTS:
        void reconfigure();
        void recalculateBorders();
        void updateButtonsGeometry();
        void updateButtonsGeometryDelayed();
        void updateTitleBar();
        void updateAnimationState();

        private:

        //* return the rect in which caption will be drawn
        QPair<QRect,Qt::Alignment> captionRect() const;

        void createButtons();
        void paintTitleBar(QPainter *painter, const QRect &repaintRegion);
        void paintTitleBarShading(QPainter *painter, const QRect &titleRect, const bool rounded);
        void smoothenTitleBarCorners(QPainter *painter, const QRect &titleRect, bool bottom);
        void createShadow();

        //*@name border size
        //@{
        int borderSize() const;
        inline bool hasBorders() const;
        inline bool hasNoBorders() const;
        inline bool hasNoSideBorders() const;
        //@}

        InternalSettingsPtr m_internalSettings;
        KDecoration2::DecorationButtonGroup *m_leftButtons = nullptr;
        KDecoration2::DecorationButtonGroup *m_rightButtons = nullptr;

        //* active state change animation
        QVariantAnimation *m_animation;

        //* active state change opacity
        qreal m_opacity = 0;

    };

    bool Decoration::hasBorders() const
    {
        if( m_internalSettings && m_internalSettings->mask() & BorderSize ) return m_internalSettings->borderSize() > InternalSettings::BorderNoSides;
        else return settings()->borderSize() > KDecoration2::BorderSize::NoSides;
    }

    bool Decoration::hasNoBorders() const
    {
        if( m_internalSettings && m_internalSettings->mask() & BorderSize ) return m_internalSettings->borderSize() == InternalSettings::BorderNone;
        else return settings()->borderSize() == KDecoration2::BorderSize::None;
    }

    bool Decoration::hasNoSideBorders() const
    {
        if( m_internalSettings && m_internalSettings->mask() & BorderSize ) return m_internalSettings->borderSize() == InternalSettings::BorderNoSides;
        else return settings()->borderSize() == KDecoration2::BorderSize::NoSides;
    }

    bool Decoration::isMaximized() const
    {
        auto clientPtr = client().toStrongRef();
        return !clientPtr.isNull() && clientPtr->isMaximized() && !m_internalSettings->drawBorderOnMaximizedWindows();
    }

    bool Decoration::isMaximizedHorizontally() const
    {
        auto clientPtr = client().toStrongRef();
        return !clientPtr.isNull() && clientPtr->isMaximizedHorizontally() && !m_internalSettings->drawBorderOnMaximizedWindows();
    }

    bool Decoration::isMaximizedVertically() const
    {
        auto clientPtr = client().toStrongRef();
        return !clientPtr.isNull() && clientPtr->isMaximizedVertically() && !m_internalSettings->drawBorderOnMaximizedWindows();
    }

    bool Decoration::isLeftEdge() const
    {
        auto clientPtr = client().toStrongRef();
        return !clientPtr.isNull() && (clientPtr->isMaximizedHorizontally() || clientPtr->adjacentScreenEdges().testFlag( Qt::LeftEdge ) ) && !m_internalSettings->drawBorderOnMaximizedWindows();
    }

    bool Decoration::isRightEdge() const
    {
        auto clientPtr = client().toStrongRef();
        return !clientPtr.isNull() && (clientPtr->isMaximizedHorizontally() || clientPtr->adjacentScreenEdges().testFlag( Qt::RightEdge ) ) && !m_internalSettings->drawBorderOnMaximizedWindows();
    }

    bool Decoration::isTopEdge() const
    {
        auto clientPtr = client().toStrongRef();
        return !clientPtr.isNull() && (clientPtr->isMaximizedVertically() || clientPtr->adjacentScreenEdges().testFlag( Qt::TopEdge ) ) && !m_internalSettings->drawBorderOnMaximizedWindows();
    }

    bool Decoration::isBottomEdge() const
    {
        auto clientPtr = client().toStrongRef();
        return !clientPtr.isNull() && (clientPtr->isMaximizedVertically() || clientPtr->adjacentScreenEdges().testFlag( Qt::BottomEdge ) ) && !m_internalSettings->drawBorderOnMaximizedWindows();
    }

    bool Decoration::hideTitleBar() const
    {
        auto clientPtr = client().toStrongRef();
        return !clientPtr.isNull() && m_internalSettings->hideTitleBar() && !clientPtr->isShaded();
    }

}

#endif
