import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: footer
    height: credits.height + Theme.paddingMedium
    width: parent ? parent.width : Screen.width
    property string username: ""
    property string permalinkUrl: ""

    Column {
        id: credits
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.horizontalPageMargin
        anchors.rightMargin: Theme.horizontalPageMargin
        spacing: Theme.paddingSmall

        Label {
            width: parent.width
            visible: footer.username.length > 0
            text: qsTr("Uploaded by %1").arg(footer.username)
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
        }
        Row {
            spacing: Theme.paddingSmall
            Icon {
                source: "image://theme/icon-s-cloud-upload"
                color: Theme.secondaryHighlightColor
                width: Theme.iconSizeSmall
                height: width
            }
            Label {
                text: "SoundCloud"
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Label {
            width: parent.width
            visible: footer.permalinkUrl.length > 0
            text: footer.permalinkUrl
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeExtraSmall
            truncationMode: TruncationMode.Fade
            MouseArea {
                anchors.fill: parent
                onClicked: Qt.openUrlExternally(footer.permalinkUrl)
            }
        }
    }
}
