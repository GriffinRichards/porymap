#pragma once
#ifndef PARSEUTIL_H
#define PARSEUTIL_H

#include "log.h"
#include "orderedjson.h"
#include "orderedmap.h"

#include <QString>
#include <QList>
#include <QMap>
#include <QRegularExpression>



enum TokenClass {
    Number,
    Operator,
    Error,
};

class Token {
public:
    Token(QString value = "", QString type = "") {
        this->value = value;
        this->type = TokenClass::Operator;
        if (type == "decimal" || type == "hex") {
            this->type = TokenClass::Number;
            this->operatorPrecedence = -1;
        } else if (type == "operator") {
            this->operatorPrecedence = precedenceMap[value];
        } else if (type == "error") {
            this->type = TokenClass::Error;
        }
    }
    static QMap<QString, int> precedenceMap;
    QString value;
    TokenClass type;
    int operatorPrecedence; // only relevant for operator tokens
};

class ParseUtil
{
public:
    ParseUtil();
    void set_root(const QString &dir);
    static QString readTextFile(const QString &path, QString *error = nullptr);
    void invalidateTextFile(const QString &path);
    static int textFileLineCount(const QString &path);
    QList<QStringList> parseAsm(const QString &filename);
    QStringList readCArray(const QString &filename, const QString &label);
    QMap<QString, QStringList> readCArrayMulti(const QString &filename);
    QMap<QString, QString> readNamedIndexCArray(const QString &text, const QString &label, QString *error = nullptr);
    QString readCIncbin(const QString &filename, const QString &label);
    QString getCIncbin(const QString &text, const QString &label);
    QMap<QString, QString> readCIncbinMulti(const QString &filepath);
    QStringList readCIncbinArray(const QString &filename, const QString &label);
    QStringList getCIncbinArray(const QString &text, const QString &label);
    QMap<QString, int> readCDefinesByRegex(const QString &filename, const QSet<QString> &regexList, QString *error = nullptr);
    QMap<QString, int> readCDefinesByName(const QString &filename, const QSet<QString> &names, QString *error = nullptr);
    QStringList readCDefineNames(const QString &filename, const QSet<QString> &regexList, QString *error = nullptr);
    tsl::ordered_map<QString, QHash<QString, QString>> readCStructs(const QString &, const QString & = "", const QHash<int, QString>& = {});
    QList<QStringList> getLabelMacros(const QList<QStringList>&, const QString&);
    QStringList getLabelValues(const QList<QStringList>&, const QString&);
    bool tryParseJsonFile(QJsonDocument *out, const QString &filepath, QString *error = nullptr);
    bool tryParseOrderedJsonFile(poryjson::Json::object *out, const QString &filepath, QString *error = nullptr);

    // Returns the 1-indexed line number for the definition of scriptLabel in the scripts file at filePath.
    // Returns 0 if a definition for scriptLabel cannot be found.
    static int getScriptLineNumber(const QString &filePath, const QString &scriptLabel);
    static int getRawScriptLineNumber(QString text, const QString &scriptLabel);
    static int getPoryScriptLineNumber(QString text, const QString &scriptLabel);
    static QStringList getGlobalScriptLabels(const QString &filePath);
    static QStringList getGlobalRawScriptLabels(QString text);
    static QStringList getGlobalPoryScriptLabels(QString text);
    static QString removeStringLiterals(QString text);
    static QString removeLineComments(QString text, const QString &commentSymbol);
    static QString removeLineComments(QString text, const QStringList &commentSymbols);

    static QStringList splitShellCommand(QStringView command);
    static int gameStringToInt(const QString &gameString, bool * ok = nullptr);
    static bool gameStringToBool(const QString &gameString, bool * ok = nullptr);
    static QString jsonToQString(const QJsonValue &value, bool * ok = nullptr);
    static int jsonToInt(const QJsonValue &value, bool * ok = nullptr);
    static bool jsonToBool(const QJsonValue &value, bool * ok = nullptr);

private:
    QString root;
    QString text;
    QString file;
    QString curDefine;
    QHash<QString, QStringList> errorMap;
    int evaluateDefine(const QString&, const QString &, QMap<QString, int>*, QMap<QString, QString>*);
    QList<Token> tokenizeExpression(QString, QMap<QString, int>*, QMap<QString, QString>*);
    QList<Token> generatePostfix(const QList<Token> &tokens);
    int evaluatePostfix(const QList<Token> &postfix);
    void recordError(const QString &message);
    void recordErrors(const QStringList &errors);
    void logRecordedErrors();
    QString createErrorMessage(const QString &message, const QString &expression);

    struct ParsedDefines {
        QMap<QString,QString> expressions; // Map of all define names encountered to their expressions
        QStringList filteredNames; // List of define names that matched the search text, in the order that they were encountered
    };
    ParsedDefines readCDefines(const QString &filename, const QSet<QString> &filterList, bool useRegex, QString *error);
    QMap<QString, int> evaluateCDefines(const QString &filename, const QSet<QString> &filterList, bool useRegex, QString *error);
    bool defineNameMatchesFilter(const QString &name, const QSet<QString> &filterList) const;
    bool defineNameMatchesFilter(const QString &name, const QSet<QRegularExpression> &filterList) const;

    static const QRegularExpression re_incScriptLabel;
    static const QRegularExpression re_globalIncScriptLabel;
    static const QRegularExpression re_poryScriptLabel;
    static const QRegularExpression re_globalPoryScriptLabel;
    static const QRegularExpression re_poryRawSection;
};

#endif // PARSEUTIL_H
