#include "common/common_pch.h"

#include <QComboBox>
#include <QDateTime>
#include <QIcon>
#include <QList>
#include <QPushButton>
#include <QString>
#include <QTreeView>

#include "common/qt.h"
#include "common/strings/editing.h"
#include "mkvtoolnix-gui/util/util.h"

namespace Util {

QIcon
loadIcon(QString const &name,
         QList<int> const &sizes) {
  QIcon icon;
  for (auto size : sizes)
    icon.addFile(QString{":/icons/%1x%1/%2"}.arg(size).arg(name));

  return icon;
}

bool
setComboBoxIndexIf(QComboBox *comboBox,
                   std::function<bool(QString const &, QVariant const &)> test) {
  auto count = comboBox->count();
  for (int idx = 0; count > idx; ++idx)
    if (test(comboBox->itemText(idx), comboBox->itemData(idx))) {
      comboBox->setCurrentIndex(idx);
      return true;
    }

  return false;
}

void
enableWidgets(QList<QWidget *> const &widgets,
              bool enable) {
  for (auto &widget : widgets)
    widget->setEnabled(enable);
}

QPushButton *
buttonForRole(QDialogButtonBox *box,
              QDialogButtonBox::ButtonRole role) {
  auto buttons = box->buttons();
  auto button  = boost::find_if(buttons, [&](QAbstractButton *b) { return box->buttonRole(b) == role; });
  return button == buttons.end() ? nullptr : static_cast<QPushButton *>(*button);
}

void
resizeViewColumnsToContents(QTreeView *view) {
  auto columnCount = view->model()->columnCount(QModelIndex{});
  for (auto column = 0; columnCount > column; ++column)
    view->resizeColumnToContents(column);
}

void
withSelectedIndexes(QAbstractItemView *view,
                    std::function<void(QModelIndex const &)> worker) {
  withSelectedIndexes(view->selectionModel(), worker);
}

void
withSelectedIndexes(QItemSelectionModel *selectionModel,
                    std::function<void(QModelIndex const &)> worker) {
  auto rowsSeen = QMap< std::pair<QModelIndex, int>, bool >{};
  for (auto const &range : selectionModel->selection())
    for (auto const &index : range.indexes()) {
      auto seenIdx = std::make_pair(index.parent(), index.row());
      if (rowsSeen[seenIdx])
        continue;
      rowsSeen[seenIdx] = true;
      worker(index.sibling(index.row(), 0));
    }
}

QModelIndex
toTopLevelIdx(QModelIndex const &idx) {
  if (!idx.isValid())
    return QModelIndex{};

  auto parent = idx.parent();
  return parent == QModelIndex{} ? idx : parent;
}

static QString
escape_mkvtoolnix(QString const &source) {
  if (source.isEmpty())
    return QString{"#EMPTY#"};
  return to_qs(::escape(to_utf8(source)));
}

static QString
unescape_mkvtoolnix(QString const &source) {
  if (source == Q("#EMPTY#"))
    return Q("");
  return to_qs(::unescape(to_utf8(source)));
}

static QString
escape_shell_unix(QString const &source) {
  if (!source.contains(QRegExp{"[^\\w%+,\\-./:=@]"}))
    return source;

  auto copy = source;
  // ' -> '\''
  copy.replace(QRegExp{"'"}, Q("'\\''"));

  copy = Q("'%1'").arg(copy);
  copy.replace(QRegExp{"^''"}, Q(""));
  copy.replace(QRegExp{"''$"}, Q(""));

  return copy;
}

static QString
escape_shell_windows(QString const &source) {
  if (!source.contains(QRegExp{"[\\w+,\\-./:=@]"}))
    return source;

  auto copy = QString{'"'};

  for (auto it = source.begin(), end = source.end() ; ; ++it) {
    QString::size_type numBackslashes = 0;

    while ((it != end) && (*it == QChar{'\\'})) {
      ++it;
      ++numBackslashes;
    }

    if (it == end) {
      copy += QString{numBackslashes * 2, QChar{'\\'}};
      break;

    } else if (*it == QChar{'"'})
      copy += QString{numBackslashes * 2 + 1, QChar{'\\'}} + *it;

    else
      copy += QString{numBackslashes, QChar{'\\'}} + *it;
  }

  copy += QChar{'"'};

  copy.replace(QRegExp{"([()%!^\"<>&|])"}, Q("^\\1"));

  return copy;
}

QString
escape(QString const &source,
       EscapeMode mode) {
  return EscapeMkvtoolnix   == mode ? escape_mkvtoolnix(source)
       : EscapeShellUnix    == mode ? escape_shell_unix(source)
       : EscapeShellWindows == mode ? escape_shell_windows(source)
       :                              source;
}

QString
unescape(QString const &source,
         EscapeMode mode) {
  Q_ASSERT(EscapeMkvtoolnix == mode);

  return unescape_mkvtoolnix(source);
}

QStringList
escape(QStringList const &source,
       EscapeMode mode) {
  auto escaped = QStringList{};
  for (auto const &string : source)
    escaped << escape(string, mode);

  return escaped;
}

QStringList
unescape(QStringList const &source,
         EscapeMode mode) {
  auto unescaped = QStringList{};
  for (auto const &string : source)
    unescaped << unescape(string, mode);

  return unescaped;
}

QString
joinSentences(QStringList const &sentences) {
  // TODO: act differently depending on the UI locale. Some languages,
  // e.g. Japanese, don't join sentences with spaces.
  return sentences.join(" ");
}

QString
displayableDate(QDateTime const &date) {
  return date.isValid() ? date.toString(QString{"yyyy-MM-dd hh:mm:ss"}) : QString{""};
}

}
