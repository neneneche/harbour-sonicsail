import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: aboutPage

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("About") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: "Soundsail"
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeLarge
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("SoundCloud client for Sailfish OS")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
            }

            SectionHeader { text: qsTr("Project") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: "https://github.com/neneneche/harbour-sonicsail"
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                MouseArea {
                    anchors.fill: parent
                    onClicked: Qt.openUrlExternally("https://github.com/neneneche/harbour-sonicsail")
                }
            }

            SectionHeader { text: qsTr("Authors") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: "neneneche\nXiaomi MiMo v2.6 Pro"
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            SectionHeader { text: qsTr("Backend") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("Uses the public SoundCloud web API (api-v2.soundcloud.com). A public client_id is discovered at runtime from SoundCloud’s web player. Optional account features sign you in on SoundCloud’s own page inside the app and reuse that browser session. Playback uses SoundCloud’s media transcodings (HLS / progressive).")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            SectionHeader { text: qsTr("Credits") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("API behavior and stream discovery based on public documentation and open-source clients, especially yt-dlp’s SoundCloud extractor and community Sailfish OS media apps (e.g. SubMusic). Sailfish Silica UI components and icon shape guidance from Jolla / Sailfish OS design. Built with Qt, QtMultimedia and libsailfishapp.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("Track playback requires attribution to the uploader and SoundCloud (SoundCloud Terms of Use).")
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
            }
        }

        VerticalScrollDecorator {}
    }
}
