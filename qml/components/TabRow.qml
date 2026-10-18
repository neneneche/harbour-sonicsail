import QtQuick 2.0
import Sailfish.Silica 1.0

Row {
    id: tabRow
    property int currentTab: 0
    signal tabSelected(int index)

    x: Theme.horizontalPageMargin
    width: parent ? parent.width - Theme.horizontalPageMargin * 2 : Screen.width
    height: Theme.itemSizeSmall

    Repeater {
        model: [
            { t: qsTr("Playlists"), i: 0 },
            { t: qsTr("Search"), i: 1 },
            { t: qsTr("Likes"), i: 2 }
        ]
        BackgroundItem {
            width: tabRow.width / 3
            height: tabRow.height
            onClicked: tabRow.tabSelected(modelData.i)
            Label {
                anchors.centerIn: parent
                text: modelData.t
                color: tabRow.currentTab === modelData.i ? Theme.highlightColor : Theme.secondaryColor
                font.bold: tabRow.currentTab === modelData.i
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
}
