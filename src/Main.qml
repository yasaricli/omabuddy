import QtQuick
import Buddy

// The whole screen, transparent. main.cpp makes it a layer-shell surface and
// limits its input to Buddy's own rectangle.
Window {
    id: win
    visible: false
    color: "transparent"
    title: "Omabuddy"

    onWidthChanged: pet.setArena(width, height)
    onHeightChanged: pet.setArena(width, height)

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        PixelSprite {
            id: sprite
            x: pet.x
            y: pet.y
            width: pet.width
            height: pet.height
            frame: pet.frame
            mirrored: pet.facingLeft
            accent: theme.accent

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                hoverEnabled: true
                preventStealing: true
                cursorShape: pet.carried ? Qt.ClosedHandCursor : Qt.OpenHandCursor

                onPressed: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        pet.farewell()
                        return
                    }
                    const p = mapToItem(null, mouse.x, mouse.y)
                    pet.press(p.x, p.y)
                }
                onPositionChanged: (mouse) => {
                    if (!(mouse.buttons & Qt.LeftButton))
                        return
                    const p = mapToItem(null, mouse.x, mouse.y)
                    pet.movePointer(p.x, p.y)
                }
                onReleased: (mouse) => {
                    if (mouse.button === Qt.LeftButton)
                        pet.release()
                }
            }
        }

        SpeechBubble {
            text: pet.bubble
            target: sprite
            arenaWidth: win.width
        }
    }
}
