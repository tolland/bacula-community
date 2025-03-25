/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2025 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/
#include "fd-config-ui-controller.h"

FdConfigUiController::FdConfigUiController(QObject *parent):
    QObject(parent)
{}

void FdConfigUiController::readFileDaemonConfig()
{
    QString fileName = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + "/etc/bacula-fd.conf";
    if (QFile::exists(fileName)) {
        QFile file(fileName);
        if (file.open(QFile::ReadOnly)) {
            QTextStream stream(&file);
            QString fdConfig = stream.readAll();
            file.close();
            emit fileDaemonConfigRead(fdConfig);
        }
    } else {
        emit fileDaemonConfigRead("ERROR - File Daemon config file not found");
    }
}

void FdConfigUiController::writeFileDaemonConfig(QString fcontents)
{

    QString filePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + "/etc/bacula-fd.conf";
    QFile file(filePath);

    if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
        return;
    }

    file.write(fcontents.toUtf8().constData());
    file.close();
}

FdConfigUiController::~FdConfigUiController()
{}
