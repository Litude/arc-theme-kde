/*
 *   Copyright 2014 Marco Martin <mart@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2,
 *   or (at your option) any later version, as published by the Free
 *   Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick 2.5
import QtQuick.Window 2.2

Rectangle {
    id: root
    color: "black"

    property int stage

    onStageChanged: {
        if (stage == 6) {
            opacityAnimation.from = 1;
            opacityAnimation.to = 0;
            opacityAnimation.duration = 250;
            pauseAnimation.duration = 250;
            introAnimation.running = true;
        }
    }
    Item {
        id: content
        anchors.fill: parent
        opacity: 0
        Rectangle {
            radius: 4
            color: "#252a35"
            anchors.centerIn: parent
            height: 8
            width: height*32
            Rectangle {
                radius: 3
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                }
                width: (parent.width / 6) * stage
                color: "#5294e2"
                Behavior on width { 
                    PropertyAnimation {
                        duration: 250
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }
    }
    
    SequentialAnimation {
        id: introAnimation
        running: true
        
        PauseAnimation {
            id: pauseAnimation
            duration: 0
        }
        
        OpacityAnimator {
            id: opacityAnimation
            target: content
            from: 0
            to: 1
            duration: 500
            easing.type: Easing.InOutQuad
        }
    }

}
