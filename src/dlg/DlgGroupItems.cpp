// DlgGroupItems.cpp
#include "dlg/DlgGroupItems.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

DlgGroupItems::DlgGroupItems(QWidget* parent, const QStringList& tabNames,
                             const QString& defaultName)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("窗口分组"));
    resize(400, 360);

    auto* vbox = new QVBoxLayout(this);

    auto* hName = new QHBoxLayout;
    hName->addWidget(new QLabel(QStringLiteral("分组名称"), this));
    m_edName = new QLineEdit(defaultName, this);
    hName->addWidget(m_edName, 1);
    vbox->addLayout(hName);

    vbox->addWidget(new QLabel(QStringLiteral("选择加入该分组的窗口："), this));

    m_list = new QListWidget(this);
    for (int i = 0; i < tabNames.size(); ++i) {
        auto* it = new QListWidgetItem(tabNames[i], m_list);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Checked);
        it->setData(Qt::UserRole, i);
    }
    vbox->addWidget(m_list, 1);

    auto* hBtn = new QHBoxLayout;
    auto* btnOk = new QPushButton(QStringLiteral("确定"), this);
    btnOk->setDefault(true);
    auto* btnCancel = new QPushButton(QStringLiteral("取消"), this);
    hBtn->addStretch(1);
    hBtn->addWidget(btnOk);
    hBtn->addWidget(btnCancel);
    vbox->addLayout(hBtn);

    connect(btnOk, &QPushButton::clicked, this, [this]{
        if (!m_edName->text().trimmed().isEmpty())
            accept();
    });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

QString DlgGroupItems::GroupName() const
{
    return m_edName->text().trimmed();
}

QVector<int> DlgGroupItems::SelectedTabs() const
{
    QVector<int> out;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* it = m_list->item(i);
        if (it->checkState() == Qt::Checked)
            out.push_back(it->data(Qt::UserRole).toInt());
    }
    return out;
}
