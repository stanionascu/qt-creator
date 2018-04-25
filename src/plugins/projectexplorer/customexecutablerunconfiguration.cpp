/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "customexecutablerunconfiguration.h"

#include "abi.h"
#include "buildconfiguration.h"
#include "customexecutableconfigurationwidget.h"
#include "devicesupport/devicemanager.h"
#include "localenvironmentaspect.h"
#include "project.h"
#include "runconfigurationaspects.h"
#include "target.h"

#include <coreplugin/icore.h>

#include <utils/qtcprocess.h>
#include <utils/stringutils.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace ProjectExplorer::Internal;

namespace ProjectExplorer {

const char CUSTOM_EXECUTABLE_ID[] = "ProjectExplorer.CustomExecutableRunConfiguration";
const char EXECUTABLE_KEY[] = "ProjectExplorer.CustomExecutableRunConfiguration.Executable";

// Dialog embedding the CustomExecutableConfigurationWidget
// prompting the user to complete the configuration.
class CustomExecutableDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CustomExecutableDialog(CustomExecutableRunConfiguration *rc, QWidget *parent = 0);

    void accept();

    bool event(QEvent *event);

private:
    void changed()
    {
        setOkButtonEnabled(m_widget->isValid());
    }

    inline void setOkButtonEnabled(bool e)
    {
        m_dialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(e);
    }

    QDialogButtonBox *m_dialogButtonBox;
    CustomExecutableConfigurationWidget *m_widget;
};

CustomExecutableRunConfiguration::CustomExecutableRunConfiguration(Target *target, Core::Id id)
    : RunConfiguration(target, id)
{
    addExtraAspect(new LocalEnvironmentAspect(this, LocalEnvironmentAspect::BaseEnvironmentModifier()));
    addExtraAspect(new ArgumentsAspect(this, "ProjectExplorer.CustomExecutableRunConfiguration.Arguments"));
    addExtraAspect(new TerminalAspect(this, "ProjectExplorer.CustomExecutableRunConfiguration.UseTerminal"));
    addExtraAspect(new WorkingDirectoryAspect(this, "ProjectExplorer.CustomExecutableRunConfiguration.WorkingDirectory"));
    setDefaultDisplayName(defaultDisplayName());
}

CustomExecutableRunConfiguration::CustomExecutableRunConfiguration(Target *target)
    : CustomExecutableRunConfiguration(target, CUSTOM_EXECUTABLE_ID)
{
}

// Note: Qt4Project deletes all empty customexecrunconfigs for which isConfigured() == false.
CustomExecutableRunConfiguration::~CustomExecutableRunConfiguration()
{
    if (m_dialog) {
        emit configurationFinished();
        disconnect(m_dialog, &QDialog::finished,
                   this, &CustomExecutableRunConfiguration::configurationDialogFinished);
    }
    delete m_dialog;
}

CustomExecutableDialog::CustomExecutableDialog(CustomExecutableRunConfiguration *rc, QWidget *parent)
    : QDialog(parent)
    , m_dialogButtonBox(new  QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel))
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *label = new QLabel(tr("Could not find the executable, please specify one."));
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(label);
    m_widget = new CustomExecutableConfigurationWidget(rc, CustomExecutableConfigurationWidget::DelayedApply);
    m_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(m_widget, &CustomExecutableConfigurationWidget::validChanged,
            this, &CustomExecutableDialog::changed);
    layout->addWidget(m_widget);
    setOkButtonEnabled(false);
    connect(m_dialogButtonBox, &QDialogButtonBox::accepted, this, &CustomExecutableDialog::accept);
    connect(m_dialogButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_dialogButtonBox);
    layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
}

void CustomExecutableDialog::accept()
{
    m_widget->apply();
    QDialog::accept();
}

bool CustomExecutableDialog::event(QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape && !ke->modifiers()) {
            ke->accept();
            return true;
        }
    }
    return QDialog::event(event);
}

// CustomExecutableRunConfiguration

RunConfiguration::ConfigurationState CustomExecutableRunConfiguration::ensureConfigured(QString *errorMessage)
{
    if (m_dialog) {// uhm already shown
        errorMessage->clear(); // no error dialog
        m_dialog->activateWindow();
        m_dialog->raise();
        return UnConfigured;
    }

    m_dialog = new CustomExecutableDialog(this, Core::ICore::mainWindow());
    connect(m_dialog, &QDialog::finished,
            this, &CustomExecutableRunConfiguration::configurationDialogFinished);
    m_dialog->setWindowTitle(displayName()); // pretty pointless
    m_dialog->show();
    return Waiting;
}

void CustomExecutableRunConfiguration::configurationDialogFinished()
{
    disconnect(m_dialog, &QDialog::finished,
               this, &CustomExecutableRunConfiguration::configurationDialogFinished);
    m_dialog->deleteLater();
    m_dialog = nullptr;
    emit configurationFinished();
}

// Search the executable in the path.
bool CustomExecutableRunConfiguration::validateExecutable(QString *executable, QString *errorMessage) const
{
    if (executable)
        executable->clear();
    if (m_executable.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("No executable.");
        return false;
    }
    Utils::Environment env;
    EnvironmentAspect *aspect = extraAspect<EnvironmentAspect>();
    if (aspect)
        env = aspect->environment();
    const Utils::FileName exec = env.searchInPath(macroExpander()->expand(m_executable),
                                                  {extraAspect<WorkingDirectoryAspect>()->workingDirectory()});
    if (exec.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("The executable\n%1\ncannot be found in the path.").
                            arg(QDir::toNativeSeparators(m_executable));
        return false;
    }
    if (executable)
        *executable = exec.toString();
    return true;
}

QString CustomExecutableRunConfiguration::executable() const
{
    QString result;
    validateExecutable(&result);
    return result;
}

QString CustomExecutableRunConfiguration::rawExecutable() const
{
    return m_executable;
}

bool CustomExecutableRunConfiguration::isConfigured() const
{
    return !m_executable.isEmpty();
}

Runnable CustomExecutableRunConfiguration::runnable() const
{
    StandardRunnable r;
    r.executable = executable();
    r.commandLineArguments = extraAspect<ArgumentsAspect>()->arguments();
    r.environment = extraAspect<LocalEnvironmentAspect>()->environment();
    r.runMode = extraAspect<TerminalAspect>()->runMode();
    r.device = DeviceManager::instance()->defaultDevice(Constants::DESKTOP_DEVICE_TYPE);
    return r;
}

QString CustomExecutableRunConfiguration::defaultDisplayName() const
{
    if (m_executable.isEmpty())
        return tr("Custom Executable");
    else
        return tr("Run %1").arg(QDir::toNativeSeparators(m_executable));
}

QVariantMap CustomExecutableRunConfiguration::toMap() const
{
    QVariantMap map(RunConfiguration::toMap());
    map.insert(QLatin1String(EXECUTABLE_KEY), m_executable);
    return map;
}

bool CustomExecutableRunConfiguration::fromMap(const QVariantMap &map)
{
    m_executable = map.value(QLatin1String(EXECUTABLE_KEY)).toString();
    setDefaultDisplayName(defaultDisplayName());
    return RunConfiguration::fromMap(map);
}

void CustomExecutableRunConfiguration::setExecutable(const QString &executable)
{
    if (executable == m_executable)
        return;
    m_executable = executable;
    setDefaultDisplayName(defaultDisplayName());
    emit changed();
}

QWidget *CustomExecutableRunConfiguration::createConfigurationWidget()
{
    return new CustomExecutableConfigurationWidget(this, CustomExecutableConfigurationWidget::InstantApply);
}

Abi CustomExecutableRunConfiguration::abi() const
{
    return Abi(); // return an invalid ABI: We do not know what we will end up running!
}

// Factory

CustomExecutableRunConfigurationFactory::CustomExecutableRunConfigurationFactory() :
    FixedRunConfigurationFactory(CustomExecutableRunConfiguration::tr("Custom Executable"))
{
    registerRunConfiguration<CustomExecutableRunConfiguration>(CUSTOM_EXECUTABLE_ID);
}

} // namespace ProjectExplorer

#include "customexecutablerunconfiguration.moc"
