#include "url.h"

#include <QDesktopServices>

const QUrl& Url::get(Url::ID id) {
    switch (id) {
    case ID::Manual:
        {static const QUrl url("https://huderlem.github.io/porymap/"); return url;}
    case ID::ManualProjectFiles:
        {static const QUrl url("https://huderlem.github.io/porymap/manual/project-files.html#files"); return url;}
    case ID::ManualProjectIdentifiers:
        {static const QUrl url("https://huderlem.github.io/porymap/manual/project-files.html#identifiers"); return url;}
    case ID::ManualPlugins:
        {static const QUrl url("https://huderlem.github.io/porymap/manual/scripting-capabilities.html"); return url;}

    // We could use the URL ".../releases/latest" to retrieve less data, but this would run into problems if the
    // most recent item on the releases page is not actually a new release (like the static windows build).
    // By getting all releases we can also present a multi-version changelog of all changes since the host release.
    case ID::Releases:
        {static const QUrl url("https://api.github.com/repos/huderlem/porymap/releases"); return url;}
    }

    static const QUrl empty;
    return empty;
}

bool Url::open(const QUrl& url) {
    return QDesktopServices::openUrl(url);
}

bool Url::open(Url::ID id) {
    return open(get(id));
}

bool Url::openLocalFile(const QString& path) {
    return open(QUrl::fromLocalFile(path));
}
