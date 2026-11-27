/**
 * Slate Browser
 * Blocklist.h - Set of ad and tracker hosts
 */

#ifndef BLOCKLIST_H
#define BLOCKLIST_H

#include <QObject>
#include <QSet>
#include <QString>

class Blocklist : public QObject
{
    Q_OBJECT
public:
    // Starts with a small built-in list so blocking works before any download
    explicit Blocklist(QObject *parent = nullptr);

    // True if host or any parent domain of it is listed
    bool contains(const QString &host) const;
    int size() const { return m_domains.size(); }

    // Add domains from a hosts-format file ("0.0.0.0 domain" or one domain
    // per line, # comments). Returns the number of new domains.
    int loadHostsFile(const QString &filePath);

    // Load the cached StevenBlack hosts list and re-download it if it is
    // missing or more than a week old.
    void syncRemoteList();

private:
    QSet<QString> m_domains;
};

#endif // BLOCKLIST_H
