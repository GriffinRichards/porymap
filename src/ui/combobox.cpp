#include "combobox.h"
#include "utility.h"

#include <QCompleter>

ComboBox::ComboBox(QWidget *parent) : QComboBox(parent) {
    // QComboBox (as of writing) has no 'editing finished' signal to capture
    // changes made either through the text edit or the drop-down.
    connect(this, QOverload<int>::of(&QComboBox::activated), this, &ComboBox::editingFinished);
}

void ComboBox::initLineEdit(QLineEdit *edit) {
    if (!edit) return;

    connect(edit, &QLineEdit::editingFinished, this, &ComboBox::editingFinished, Qt::UniqueConnection);

    if (edit->completer()) {
        // Allow items to be searched by any part of the word, displaying all matches.
        // The default completer behavior is not particularly good, so we do this for all our combo boxes.
        edit->completer()->setCompletionMode(QCompleter::PopupCompletion);
        edit->completer()->setFilterMode(Qt::MatchContains);
    }
}

void ComboBox::setEditable(bool editable) {
    QComboBox::setEditable(editable);

    // Changing editability will either create or destroy the line edit.
    // If it was newly-created it needs to be connected.
    if (editable) initLineEdit(lineEdit());
}

void ComboBox::setLineEdit(QLineEdit *edit) {
    QComboBox::setLineEdit(edit);
    initLineEdit(edit);
}

void ComboBox::setItem(int index, const QString &text) {
    if (index >= 0) {
        // Valid item
        setCurrentIndex(index);
    } else if (isEditable()) {
        // Invalid item in editable box, just display the text
        setCurrentText(text);
    } else {
        // Invalid item in uneditable box, display text as placeholder
        // On Qt < 5.15 this will display an empty box
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        setPlaceholderText(text);
#endif
        setCurrentIndex(index);
    }
}

void ComboBox::setTextItem(const QString &text){
    setItem(findText(text), text);
}

void ComboBox::setNumberItem(int value) {
    setItem(findData(value), QString::number(value));
}

void ComboBox::setHexItem(uint32_t value) {
    setItem(findData(value), Util::toHexString(value));
}

void ComboBox::setClearButtonEnabled(bool enabled) {
    if (lineEdit()) lineEdit()->setClearButtonEnabled(enabled);
}
