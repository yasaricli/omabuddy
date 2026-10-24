import QtQuick

// What Buddy is saying, above his head and kept on screen.
Item {
    id: bubble

    property string text
    property Item target
    property real arenaWidth

    readonly property real centerX: target.x + target.width / 2

    visible: shown.length > 0
    width: label.width + 22
    height: label.height + 12
    x: Math.max(4, Math.min(arenaWidth - width - 4, centerX - width / 2))
    y: Math.max(4, target.y - height - 10)

    // Keeps the last words on screen while the bubble fades out.
    property string shown
    onTextChanged: {
        if (text.length > 0) {
            // A fade still running would blank the new words when it ends.
            fade.stop()
            shown = text
            pop.restart()
        } else {
            fade.restart()
        }
    }

    Rectangle {
        id: tail
        width: 10
        height: 10
        rotation: 45
        color: body.color
        border.color: body.border.color
        border.width: body.border.width
        x: Math.max(body.radius + 2, Math.min(bubble.width - width - body.radius - 2, bubble.centerX - bubble.x - width / 2))
        y: bubble.height - height / 2 - 1
    }

    Rectangle {
        id: body
        anchors.fill: parent
        radius: theme.radius
        color: "#f2141413"
        border.color: theme.accent
        border.width: 2

        // Covers the tail's upper half, so the bubble and tail read as one shape.
        Rectangle {
            x: tail.x - 2
            y: parent.height - 6
            width: tail.width + 4
            height: 6
            color: parent.color
        }

        Text {
            id: label
            anchors.centerIn: parent
            text: bubble.shown
            width: Math.min(implicitWidth, 300)
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            horizontalAlignment: Text.AlignHCenter
            color: "#f0eee6"
            font.family: "monospace"
            font.pixelSize: 15
            font.bold: true
        }
    }

    ParallelAnimation {
        id: pop
        NumberAnimation { target: bubble; property: "opacity"; from: 0; to: 1; duration: 120 }
        NumberAnimation { target: bubble; property: "scale"; from: 0.7; to: 1; duration: 160; easing.type: Easing.OutBack }
    }
    SequentialAnimation {
        id: fade
        NumberAnimation { target: bubble; property: "opacity"; to: 0; duration: 180 }
        ScriptAction { script: bubble.shown = "" }
    }
}
