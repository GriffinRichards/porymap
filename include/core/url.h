#pragma once
#ifndef URL_H
#define URL_H

#include <QUrl>
#include <QString>

/*
    Some basic URL functions.
    This mostly exists to collect all our explicit URLs in one place, in case they ever need to be changed.
*/

namespace Url {
    enum ID {
        Manual,
        ManualProjectFiles,
        ManualProjectIdentifiers,
        ManualPlugins,
        Releases,
    };
    const QUrl& get(ID id);

    // Open's the specified URL using the system's default browser.
    // Returns true if the request was sent (but the operation was not necessarily successful).
    bool open(const QUrl& url);
    bool open(ID id);

    // Open's the specified local file using the system's default application.
    // Returns true if the request was sent (but the operation was not necessarily successful).
    bool openLocalFile(const QString& path);
};

#endif // URL_H
