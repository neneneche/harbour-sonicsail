import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: loginPage

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Sign in")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("Sign in with your SoundCloud browser session. Browse and play work without signing in.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Import cookies from browser")
                enabled: !auth.busy
                onClicked: auth.importBrowserCookies()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("Opens the Sailfish browser cookie store and picks up your soundcloud.com login. Sign in at soundcloud.com in the browser first.")
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("Or paste an oauth_token cookie value manually:")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            TextField {
                id: tokenField
                width: parent.width
                label: qsTr("OAuth token")
                placeholderText: qsTr("oauth_token cookie value")
                echoMode: TextInput.Password
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Sign in")
                enabled: tokenField.text.length > 0 && !auth.busy
                onClicked: auth.loginWithToken(tokenField.text)
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: auth.busy
                size: BusyIndicatorSize.Medium
            }

            Label {
                id: errorLabel
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                visible: text.length > 0
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Continue without signing in")
                onClicked: pageStack.pop()
            }
        }

        VerticalScrollDecorator {}
    }

    Connections {
        target: auth
        onLoginSucceeded: {
            settings.notify(qsTr("Signed in"))
            pageStack.pop()
        }
        onLoginFailed: errorLabel.text = message
    }
}
