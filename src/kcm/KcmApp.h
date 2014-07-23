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

#ifndef KcmApp_H
#define KcmApp_H

#include <QApplication>
#include <QMainWindow>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>

#define UI_Entry(name, type, default, description) \
    SDDM::ConfigInput<type> name ## _input { this, &name };

#include "ConfigReader.h"

namespace SDDM {
    class ConfigInputBase {
    public:
        virtual QWidget *widget() = 0;
    };

    extern QMap<ConfigSection*, QMap<ConfigEntryBase*, ConfigInputBase*>> entries;

    template <class T>
    class ConfigInput : public ConfigInputBase {
    public:
        ConfigInput(SDDM::ConfigSection *section, SDDM::ConfigEntry<T> *entry)
                : m_widget(nullptr)
                , m_entry(entry) {
            SDDM::entries[section][entry] = this;
        }
        virtual QWidget *widget() {
            if (!m_widget)
                m_widget = new QWidget;
            m_widget->setLayout(new QHBoxLayout(m_widget));
            m_widget->layout()->addWidget(new QLabel(m_entry->description().split('\n').first(), m_widget));
            m_widget->layout()->addWidget(new QLineEdit(m_entry->value(), m_widget));
            return m_widget;
        }
    private:
        QWidget *m_widget;
        SDDM::ConfigEntry<T> *m_entry;
    };

    class KcmApp : public QApplication
    {
        Q_OBJECT
    public:
        KcmApp(int& argc, char** argv);
    private:
        QMainWindow *m_mainWindow;
    };
}

#include "Configuration.h"

#endif // KcmApp_H
