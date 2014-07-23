/*
 * Runtime generated configuration UI
 * Copyright (C) 2014 Martin Bříza <mbriza@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "KcmApp.h"

#include <QtCore/QDebug>
#include <QtGui/QMainWindow>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>

namespace SDDM {
    QMap<ConfigSection*, QMap<ConfigEntryBase*, ConfigInputBase*>> entries;
    MainConfig mainConfig;

    template <> QWidget *ConfigInput<bool>::widget() {
        if (!m_widget)
            m_widget = new QWidget;
        m_widget->setLayout(new QHBoxLayout(m_widget));
        QCheckBox *chb = new QCheckBox(m_entry->description().split('\n').first(), m_widget);
        chb->setCheckState(m_entry->get() ? Qt::Checked : Qt::Unchecked);
        m_widget->layout()->addWidget(chb);
        return m_widget;
    }

    template <> QWidget *ConfigInput<MainConfig::NumState>::widget() {
        if (!m_widget)
            m_widget = new QWidget;
        m_widget->setLayout(new QHBoxLayout(m_widget));
        m_widget->layout()->addWidget(new QLabel(m_entry->description().split('\n').first(), m_widget));
        QComboBox *cbb = new QComboBox(m_widget);
        cbb->addItem("Don't touch");
        cbb->addItem("On");
        cbb->addItem("Off");
        cbb->setCurrentIndex(m_entry->get());
        m_widget->layout()->addWidget(cbb);
        return m_widget;
    }

    template <> QWidget *ConfigInput<int>::widget() {
        if (!m_widget)
            m_widget = new QWidget;
        m_widget->setLayout(new QHBoxLayout(m_widget));
        m_widget->layout()->addWidget(new QLabel(m_entry->description().split('\n').first(), m_widget));
        QSpinBox *spb = new QSpinBox(m_widget);
        spb->setMinimum(INT_MIN);
        spb->setMaximum(INT_MAX);
        spb->setValue(m_entry->get());
        m_widget->layout()->addWidget(spb);
        return m_widget;
    }

    KcmApp::KcmApp(int& argc, char** argv) 
            : QApplication(argc, argv)
            , m_mainWindow(new QMainWindow()) {
        QTabWidget *tabWidget = new QTabWidget();
        m_mainWindow->setCentralWidget(tabWidget);
        for (ConfigSection *s : entries.keys()) {
            QWidget *sectionTab = new QWidget();
            sectionTab->setLayout(new QVBoxLayout(sectionTab));
            tabWidget->addTab(sectionTab, s->name());
            for (ConfigInputBase *b : entries[s]) {
                sectionTab->layout()->addWidget(b->widget());
            }
        }
        m_mainWindow->show();
    }
}

int main(int argc, char** argv) {
    SDDM::KcmApp app(argc, argv);
    return app.exec();
}

