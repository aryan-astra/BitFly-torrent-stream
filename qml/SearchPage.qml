pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var controller
    property bool configuring: controller.searchFeed.length === 0
    signal streamChosen()

    function humanSize(bytes) {
        const gib = Number(bytes || 0) / 1073741824
        return gib >= 1 ? gib.toFixed(1) + " GiB" : Math.round(gib * 1024) + " MiB"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 30
        anchors.rightMargin: 30
        anchors.topMargin: 8
        anchors.bottomMargin: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Your indexers, one search."
                font.family: Theme.displayFamily
                font.pixelSize: 23
                color: Theme.ink
            }
            Item { Layout.fillWidth: true }
            LinkButton {
                text: root.configuring ? "Hide connection" : "Connection settings"
                onClicked: root.configuring = !root.configuring
            }
        }

        Rectangle {
            visible: root.configuring
            Layout.fillWidth: true
            implicitHeight: connectionBody.implicitHeight + 24
            color: Theme.surface
            radius: Theme.radiusLarge
            border.color: Theme.line
            ColumnLayout {
                id: connectionBody
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 7
                Label {
                    Layout.fillWidth: true
                    text: "Connect a Torznab feed from Jackett or Prowlarr. Searches use only the indexers you configure there."
                    color: Theme.secondary
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                }
                Label { text: "Torznab feed URL"; color: Theme.ink; font.pixelSize: 14 }
                TextField {
                    id: feed
                    objectName: "searchFeedInput"
                    Layout.fillWidth: true
                    text: root.controller.searchFeed
                    placeholderText: "http://127.0.0.1:9117/api/v2.0/indexers/all/results/torznab"
                    selectByMouse: true
                    Accessible.name: "Torznab feed URL"
                }
                Label { text: "API key"; color: Theme.ink; font.pixelSize: 14 }
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: key
                        objectName: "searchKeyInput"
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: root.controller.searchKeySaved ? "Key saved · leave blank to keep it" : "Paste your indexer's API key"
                        Accessible.name: "Indexer API key"
                        onVisibleChanged: if (!visible) clear()
                    }
                    QuietButton {
                        objectName: "saveSearchConnection"
                        text: "Save connection"
                        enabled: feed.text.trim().length > 0 && !root.controller.searchBusy
                        onClicked: {
                            root.controller.configureSearch(feed.text.trim(), key.text)
                            key.clear()
                        }
                    }
                }
                RowLayout {
                    LinkButton { text: "Jackett setup"; onClicked: Qt.openUrlExternally("https://github.com/Jackett/Jackett#installation-on-windows") }
                    LinkButton { text: "Prowlarr setup"; onClicked: Qt.openUrlExternally("https://wiki.servarr.com/prowlarr/installation/windows") }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: query
                objectName: "torrentSearchInput"
                Layout.fillWidth: true
                placeholderText: "Search for a title…"
                selectByMouse: true
                Accessible.name: "Torrent search query"
                onAccepted: if (search.enabled) search.clicked()
            }
            QuietButton {
                id: search
                objectName: "torrentSearchButton"
                highlighted: true
                text: "Search"
                enabled: !root.controller.searchBusy && query.text.trim().length > 0 && root.controller.searchFeed.length > 0
                onClicked: root.controller.search(query.text.trim())
            }
            QuietButton {
                visible: root.controller.searchBusy
                text: "Cancel"
                onClicked: root.controller.cancelSearch()
            }
        }
        Label {
            Layout.fillWidth: true
            text: root.controller.searchBusy ? "Searching your connected indexers…" : root.controller.searchMessage
            color: Theme.secondary
            font.pixelSize: 14
            wrapMode: Text.Wrap
            Accessible.role: Accessible.StaticText
        }

        ListView {
            id: results
            objectName: "torrentSearchResults"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: root.controller.searchResults
            reuseItems: true
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Rectangle {
                id: result
                required property int index
                required property var modelData
                width: results.width
                implicitHeight: 84
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.line
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 5
                        Label {
                            Layout.fillWidth: true
                            text: result.modelData.title || "Untitled result"
                            color: Theme.ink
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.humanSize(result.modelData.size) + " · " + (result.modelData.seeders || 0) + " seeders"
                                  + (result.modelData.provider ? " · " + result.modelData.provider : "")
                            color: Theme.secondary
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }
                    QuietButton {
                        text: "Add stream"
                        enabled: !root.controller.searchBusy
                        onClicked: { root.controller.openSearchResult(result.index); root.streamChosen() }
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                width: Math.min(380, parent.width - 30)
                visible: results.count === 0 && !root.controller.searchBusy
                text: root.controller.searchFeed.length ? "Search your connected indexers above. Results appear here." : "Connect an indexer to start searching inside BitFly."
                color: Theme.secondary
                font.pixelSize: 14
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
