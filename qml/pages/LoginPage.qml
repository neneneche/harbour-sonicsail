import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.WebEngine 1.0

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
                text: qsTr("Sign in with your SoundCloud account. Browse and play work without signing in.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Sign in in app")
                enabled: !auth.busy
                onClicked: pageStack.push(Qt.resolvedUrl("WebLoginPage.qml"))
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("Opens SoundCloud's own sign-in page. Your session is picked up automatically once you are signed in.")
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Sign in with another account")
                enabled: !auth.busy
                onClicked: {
                    // clear the embedded browser session first (same gecko
                    // mechanism as the Sailfish browser + disk wipe), so the
                    // sign-in page opens fresh instead of auto-logging-in
                    if (auth.loggedIn)
                        auth.logout()
                    WebEngine.notifyObservers("clear-private-data", "cookies-and-site-data")
                    auth.clearWebProfileSession()
                    settings.notify(qsTr("Browser session cleared"))
                    pageStack.push(Qt.resolvedUrl("WebLoginPage.qml"))
                }
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

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Clear browser session")
                enabled: !auth.busy
                onClicked: {
                    // same mechanism the Sailfish browser uses, then a disk wipe
                    WebEngine.notifyObservers("clear-private-data", "cookies-and-site-data")
                    var n = auth.clearWebProfileSession()
                    if (n > 0)
                        settings.notify(qsTr("Cleared %1 browser session items. Restart Soundsail, then sign in.").arg(n))
                    else
                        settings.notify(qsTr("Browser session cleared."))
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("Erases the saved in-app browser session (cookies and site data). To switch account, prefer “Sign in with another account” above — it works without restarting.")
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
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

    // Close deterministically once signed in: pop when we are the top page
    // (paste flow) or when WebLoginPage above us has just closed.
    onStatusChanged: {
        if (status === PageStatus.Active && auth.loggedIn
                && pageStack.currentPage === loginPage)
            pageStack.pop()
    }

    Connections {
        target: auth
        onLoginSucceeded: {
            settings.notify(qsTr("Signed in"))
            if (pageStack.currentPage === loginPage)
                pageStack.pop()
        }
        onLoginFailed: errorLabel.text = message
    }
}
