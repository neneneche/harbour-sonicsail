import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.WebView 1.0

// Writes (like / repost / follow) must come from a real browser context:
// SoundCloud's bot protection rejects plain HTTP writes (the same reason
// inkitori/soundcloud-desktop performs writes through a hidden WebView).
//
// The WebView is LAZY: it is not created at app startup (a second WebView
// running alongside the sign-in page glitches the embedded engine) — only
// on the first actual write, after the sign-in browser is closed.
Item {
    id: webWriter
    width: 8
    height: 8
    opacity: 0

    property var pending: []

    Loader {
        id: webViewLoader
        anchors.fill: parent
        active: false
        sourceComponent: Component {
            WebView {
                active: true
                url: "https://soundcloud.com"
                onLoadedChanged: {
                    if (loaded)
                        webWriter.flush()
                }
            }
        }
    }

    function ensureWebView() {
        if (!webViewLoader.active)
            webViewLoader.active = true
        return webViewLoader.item
    }

    function withClient(url) {
        var cid = String(api.publicClientId || "")
        if (cid.length === 0 || cid === "undefined" || cid === "null")
            return url
        return url + (url.indexOf("?") >= 0 ? "&" : "?") + "client_id=" + encodeURIComponent(cid)
    }

    // ---- public helpers -------------------------------------------------
    function like(trackId, on, cb) {
        write(on ? "PUT" : "DELETE",
              withClient("https://api-v2.soundcloud.com/users/" + String(auth.userId || "0") + "/track_likes/" + trackId),
              null, cb)
    }

    function repost(trackId, on, cb) {
        write(on ? "PUT" : "DELETE",
              withClient("https://api-v2.soundcloud.com/me/track_reposts/" + trackId),
              null, cb)
    }

    function follow(userId, on, cb) {
        write(on ? "POST" : "DELETE",
              withClient("https://api-v2.soundcloud.com/me/followings/" + userId),
              null, cb)
    }

    // Cached SoundCloud web app version (required by comment writes)
    property string appVersion: ""
    property var appVersionWaiters: []

    function ensureAppVersion(cb) {
        if (appVersion.length > 0) {
            cb(appVersion)
            return
        }
        appVersionWaiters.push(cb)
        if (appVersionWaiters.length > 1)
            return
        var wv = ensureWebView()
        if (!wv || !wv.loaded) {
            // retry on next write/flush; give up after short delay with fallback
            pollTimer.pendingCb = null
            versionTimer.restart()
            return
        }
        wv.runJavaScript(
            "var m = /\"appVersion\":\"(\\d+)\"/.exec(document.documentElement.innerHTML);" +
            "return m ? m[1] : '';",
            function(result) {
                var v = String(result === undefined || result === null ? "" : result)
                if (v.length === 0)
                    v = "1790934937" // known-good fallback
                appVersion = v
                flushAppVersion()
            })
    }

    function flushAppVersion() {
        var waiters = appVersionWaiters
        appVersionWaiters = []
        for (var i = 0; i < waiters.length; i++)
            waiters[i](appVersion)
    }

    function comment(trackId, body, cb) {
        var id = String(trackId || "")
        var c = id.lastIndexOf(":")
        if (c >= 0)
            id = id.substring(c + 1)
        ensureAppVersion(function(ver) {
            // web-client shape: flat query params, no body
            var url = withClient("https://api-v2.soundcloud.com/tracks/" + id + "/comments")
            url += "&app_version=" + encodeURIComponent(ver)
            url += "&body=" + encodeURIComponent(body)
            url += "&timestamp=0"
            write("POST", url, null, function(ok, info) {
                if (ok) {
                    if (cb) cb(true, info)
                    return
                }
                // silent fallback: official JSON shape
                var u2 = withClient("https://api-v2.soundcloud.com/tracks/"
                                    + encodeURIComponent("soundcloud:tracks:" + id) + "/comments")
                write("POST", u2, { comment: { body: body, timestamp: 0 } }, cb)
            })
        })
    }

    // ---- XHR in page context -------------------------------------------
    function write(method, url, body, cb, contentType) {
        var wv = ensureWebView()
        if (!wv || !wv.loaded) {
            pending.push({ m: method, u: url, b: body, cb: cb, ct: contentType })
            return
        }
        runXhr(method, url, body, cb, contentType)
    }

    function flush() {
        var wv = webViewLoader.item
        if (!wv || !wv.loaded)
            return
        var items = pending
        pending = []
        for (var i = 0; i < items.length; i++)
            runXhr(items[i].m, items[i].u, items[i].b, items[i].cb, items[i].ct)
    }

    function runXhr(method, url, body, cb, contentType) {
        var wv = webViewLoader.item
        if (!wv) {
            if (cb) cb(false, "no webview")
            return
        }
        // NOTE: runJavaScript requires a TOP-LEVEL return statement.
        var token = String(auth.accessToken || "")
        // object bodies are JSON.stringify'd inside the page (single encode)
        var isObj = body !== null && body !== undefined && typeof body === "object"
        var sendExpr = "null"
        if (isObj)
            sendExpr = "JSON.stringify(" + JSON.stringify(body) + ")"
        else if (body !== null && body !== undefined && String(body).length > 0)
            sendExpr = JSON.stringify(String(body))
        var ct = contentType ? contentType : "application/json; charset=utf-8"
        var js = ""
        js += "var x = new XMLHttpRequest();\n"
        js += "x.open('" + method + "', " + JSON.stringify(url) + ", true);\n"
        js += "x.withCredentials = true;\n"
        if (token.length > 0)
            js += "x.setRequestHeader('Authorization', 'OAuth ' + " + JSON.stringify(token) + ");\n"
        js += "x.setRequestHeader('Content-Type', " + JSON.stringify(ct) + ");\n"
        js += "x.setRequestHeader('Accept', 'application/json');\n"
        js += "x.onload = function() { window.__scWriteResult = x.status + '|' + x.responseText; };\n"
        js += "x.onerror = function() { window.__scWriteResult = '0|xhr-error'; };\n"
        js += "try { x.send(" + sendExpr + "); }\n"
        js += "catch (e) { return '0|' + e; }\n"
        js += "return 'pending';\n"

        wv.runJavaScript(js, function() {
            pollResult(cb, 0)
        })
    }

    function pollResult(cb, attempt) {
        var wv = webViewLoader.item
        if (!wv) {
            if (cb) cb(false, "no webview")
            return
        }
        wv.runJavaScript("return window.__scWriteResult || 'pending';", function(result) {
            var s = String(result === undefined || result === null ? "" : result)
            if (s === "pending" || s.length === 0) {
                if (attempt < 25) {
                    pollTimer.pendingCb = cb
                    pollTimer.pendingAttempt = attempt + 1
                    pollTimer.restart()
                } else if (cb) {
                    cb(false, "timeout")
                }
                return
            }
            wv.runJavaScript("window.__scWriteResult = undefined; return 1;", function() {})
            var bar = s.indexOf("|")
            var status = parseInt(bar >= 0 ? s.substring(0, bar) : s, 10)
            var ok = (status === 200 || status === 201 || status === 204)
            if (cb)
                cb(ok, bar >= 0 ? s.substring(bar + 1, Math.min(s.length, 160)) : s)
        })
    }

    Timer {
        id: pollTimer
        property var pendingCb
        property int pendingAttempt: 0
        interval: 200
        onTriggered: pollResult(pendingCb, pendingAttempt)
    }

    Timer {
        id: versionTimer
        interval: 1500
        onTriggered: {
            // webview not ready yet — proceed with fallback version
            appVersion = "1790934937"
            flushAppVersion()
        }
    }
}
