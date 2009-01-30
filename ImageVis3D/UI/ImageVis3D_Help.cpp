/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/


//!    File   : ImageVis3D_Help.cpp
//!    Author : Jens Krueger
//!             SCI Institute
//!             University of Utah
//!    Date   : January 2009
//
//!    Copyright (C) 2008 SCI Institute

#include "ImageVis3D.h"
#include <QtGui/QMessageBox>
#include <QtNetwork/QHttp>
#include <QtNetwork/QHttpResponseHeader>
#include <QtNetwork/QAuthenticator>
#include <QtCore/QUrl>
#include <QtCore/QTime>
#include <QtCore/QDate>
#include <QtCore/QDir>
#include <QtCore/QTemporaryFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryFile>
#include <QtCore/QTextStream>
#include <QtCore/QSettings>

#include "FTPDialog.h"
#include "../Tuvok/Basics/SysTools.h"

using namespace std;

void MainWindow::ShowAbout()
{
  QString qstrTitle;
  QString qstrText("");
#ifdef _DEBUG
  qstrTitle = tr("ImageVis3D %1 %2 DEBUG VERSION!").arg(IV3D_VERSION).arg(IV3D_VERSION_TYPE);
  qstrText = tr("Warning: this is a DEBUG build!  This version is for "
                "testing only.  Some functions may run with dramatically "
                "reduced performance.  Please download a release build for "
                "general use.\n\n");
#else
  qstrTitle = tr("ImageVis3D %1").arg(IV3D_VERSION);
#endif
  qstrText += tr("This is ImageVis3D %1 %2, using the Tuvok render engine "
                 "%3 %4 %5.  Copyright 2008,2009 by the Scientific Computing "
                 "and Imaging (SCI) Institute.  Please report bugs to "
                 "iv3d-users@sci.utah.edu")
                    .arg(IV3D_VERSION)
                    .arg(IV3D_VERSION_TYPE)
                    .arg(TUVOK_VERSION)
                    .arg(TUVOK_VERSION_TYPE)
                    .arg(TUVOK_DETAILS);
  QMessageBox::about(this, qstrTitle,qstrText);
}

void MainWindow::QuietCheckForUpdates() {
  m_bStartupCheck = true;
  CheckForUpdatesInternal();
}


void MainWindow::CheckForUpdates() {
  m_bStartupCheck = false;
  CheckForUpdatesInternal();
}

void MainWindow::CheckForUpdatesInternal() {
  // cleanup updatefile, this codepath is taken for instance when the windows firewall blocked an http request
  if (m_pUpdateFile && m_pUpdateFile->isOpen()) {
    m_pUpdateFile->close();
    m_pUpdateFile->remove();
    delete m_pUpdateFile;
    m_pUpdateFile = NULL;

    // is seems like m_pHttp gets stuck if the firewall blocked it the first time so lets create a new one
    disconnect(m_pHttp, SIGNAL(requestFinished(int, bool)), this, SLOT(httpRequestFinished(int, bool)));
    disconnect(m_pHttp, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)), this, SLOT(readResponseHeader(const QHttpResponseHeader &)));
    delete m_pHttp;

    m_pHttp = new QHttp(this);
    connect(m_pHttp, SIGNAL(requestFinished(int, bool)), this, SLOT(httpRequestFinished(int, bool)));
    connect(m_pHttp, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)), this, SLOT(readResponseHeader(const QHttpResponseHeader &)));
  }

  if (!m_pHttp) return;
  QString strCompleteFile = tr("%1%2").arg(UPDATE_VERSION_PATH).arg(UPDATE_VERSION_FILE);
  QUrl url(strCompleteFile);

  QHttp::ConnectionMode mode = url.scheme().toLower() == "https" ? QHttp::ConnectionModeHttps : QHttp::ConnectionModeHttp;
  m_pHttp->setHost(url.host(), mode, url.port() == -1 ? 0 : url.port());
  if (!url.userName().isEmpty()) m_pHttp->setUser(url.userName(), url.password());

  QString strUpdateFile = tr("%1/ImageVis3D_UpdateCheck_Temp").arg(QDir::tempPath());

  m_pUpdateFile = new QTemporaryFile(strUpdateFile);
  QByteArray remotePath = QUrl::toPercentEncoding(url.path(), "!$&'()*+,;=:@/");
  if (remotePath.isEmpty()) remotePath = "/";
  m_iHttpGetId = m_pHttp->get(remotePath, m_pUpdateFile);
}

void MainWindow::httpRequestFinished(int requestId, bool error) {
  if (requestId != m_iHttpGetId) return;

  if (m_pUpdateFile && m_pUpdateFile->isOpen()) {
    m_pUpdateFile->close();
  }

  if (error) {
    if (!m_bStartupCheck) ShowInformationDialog( tr("Update Check"),tr("Download failed: %1.").arg(m_pHttp->errorString()));
  } else {
    float fIV3DVersion = 0;
    int iIV3DSVNVersion = 0;
    float fTuvokVersion = 0;
    int iTuvokSVNVersion = 0;
    if (GetVersionsFromUpdateFile(string(m_pUpdateFile->fileName().toAscii()), fIV3DVersion, iIV3DSVNVersion, fTuvokVersion, iTuvokSVNVersion)) {
      if (fIV3DVersion > float(IV3D_VERSION) || fTuvokVersion > float(TUVOK_VERSION)) {
        QString qstrMessage = tr("A new version of ImageVis3D was found. You SHOULD download the newer version at %1").arg(UPDATE_STABLE_PATH);
        ShowInformationDialog( tr("Update Check"),qstrMessage);
      } else {
#if defined(IV3D_SVN_VERSION) && defined(TUVOK_SVN_VERSION)
        if (m_bCheckForDevBuilds && (iIV3DSVNVersion > int(IV3D_SVN_VERSION) || iTuvokSVNVersion > int(TUVOK_SVN_VERSION))) {
          QString qstrMessage = tr("A new SVN devbuild of ImageVis3D was found. You MAY want to download the newer version at %1%2").arg(UPDATE_NIGHTLY_PATH).arg(UPDATE_FILE);
          ShowInformationDialog( tr("Update Check"),qstrMessage);
        } else {
#endif
          if (!m_bStartupCheck) ShowInformationDialog( tr("Update Check"),tr("This is the most current version of ImageVis3D and Tuvok!"));
#if defined(IV3D_SVN_VERSION) && defined(TUVOK_SVN_VERSION)
        }
#endif
      }
    }
  }

  if (m_pUpdateFile) {
    m_pUpdateFile->remove();
    delete m_pUpdateFile;
    m_pUpdateFile = NULL;
  }
}

void MainWindow::readResponseHeader(const QHttpResponseHeader &responseHeader) {
  switch (responseHeader.statusCode()) {
    case 200:                   // Ok
    case 301:                   // Moved Permanently
    case 302:                   // Found
    case 303:                   // See Other
    case 307:                   // Temporary Redirect
       // these are not error conditions
       break;
    default:
       if (!m_bStartupCheck) ShowInformationDialog( tr("Update Check"), tr("Download failed: %1.") .arg(responseHeader.reasonPhrase()));
       m_pHttp->abort();
  }
}

bool MainWindow::GetVersionsFromUpdateFile(const string& strFilename, float& fIV3DVersion, int& iIV3DSVNVersion, float& fTuvokVersion, int& iTuvokSVNVersion) {
  string line ="";
  ifstream updateFile(strFilename.c_str(),ios::binary);

  if (updateFile.is_open())
  {
    if(!updateFile.eof()) { getline (updateFile,line); if(!SysTools::FromString(fIV3DVersion,line)) {updateFile.close(); return false;}} else {updateFile.close(); return false;}
    if(!updateFile.eof()) { getline (updateFile,line); if(!SysTools::FromString(iIV3DSVNVersion,line)) {updateFile.close(); return false;}} else {updateFile.close(); return false;}
    if(!updateFile.eof()) { getline (updateFile,line); if(!SysTools::FromString(fTuvokVersion,line)) {updateFile.close(); return false;}} else {updateFile.close(); return false;}
    if(!updateFile.eof()) { getline (updateFile,line); if(!SysTools::FromString(iTuvokSVNVersion,line)) {updateFile.close(); return false;}} else {updateFile.close(); return false;}
  } else return false;
  updateFile.close();

  return true;
}

void MainWindow::UploadLogToServer() {
  QFile* pFTPTempFile =new QFile("SubmittedDebugOutput.txt");

  if (!pFTPTempFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
     ShowInformationDialog("Transfer failed", "Transfer failed");
     return;
  }
  QTextStream outstream(pFTPTempFile);

  for (int i = 0;i<listWidget_DebugOut->count();i++)
    outstream << listWidget_DebugOut->item(i)->text() << "\n";
  pFTPTempFile->close();

  string strSourceName = string(pFTPTempFile->fileName().toAscii());

  delete pFTPTempFile;

  QString qstrID = tr("DebugOut_%1_%2.txt").arg(QTime::currentTime().toString()).arg(QDate::currentDate().toString());
  FtpTransfer(strSourceName, string(qstrID.toAscii()));
}

bool MainWindow::FtpTransfer(string strSource, string strDest, bool bDeleteSource) {
  if (!m_bFTPFinished) return false;
  m_bFTPFinished = true;

  if (m_pFTPDialog) {
    disconnect(m_pFTPDialog, SIGNAL(TransferFailure()), this, SLOT(FtpFail()));
    disconnect(m_pFTPDialog, SIGNAL(TransferSuccess()), this, SLOT(FtpSuccess()));
    delete m_pFTPDialog;
  }

  m_bFTPDeleteSource = bDeleteSource;
  string strFullDest = string(DEBUG_DUMP_PATH) + strDest;

  m_pFTPDialog = new FTPDialog(strSource, DEBUG_DUMP_SERVER ,strFullDest, this);

  connect(m_pFTPDialog, SIGNAL(TransferFailure()), this, SLOT(FtpFail()));
  connect(m_pFTPDialog, SIGNAL(TransferSuccess()), this, SLOT(FtpSuccess()));

  m_pFTPDialog->Start();

  return true;
}

void MainWindow::FtpFail() {
  if (SysTools::FileExists(m_strFTPTempFile) && m_bFTPDeleteSource) remove(m_strFTPTempFile.c_str());
  ShowInformationDialog("Transfer failed", "Transfer failed");
  m_bFTPFinished = true;
}

void MainWindow::FtpSuccess() {
  if (SysTools::FileExists(m_strFTPTempFile) && m_bFTPDeleteSource) remove(m_strFTPTempFile.c_str());
  ShowInformationDialog("Transfer successfull", "Transfer successfull");
  m_bFTPFinished = true;
}

void MainWindow::OnlineHelp() {
#ifdef TUVOK_OS_WINDOWS
  ShellExecuteA(NULL, "open", HELP_URL, NULL,NULL,SW_SHOWDEFAULT);
#endif
#ifdef TUVOK_OS_APPLE
  system("start "HELP_URL);
#endif
#ifdef TUVOK_OS_LINUX
  system("firefox "HELP_URL);  // TODO: Tom: instead of hoping for firefox to be installed integrate this into the UI
#endif
}

void MainWindow::OnlineVideoTut() {
#ifdef TUVOK_OS_WINDOWS
  ShellExecuteA(NULL, "open", TUTORIAL_URL, NULL,NULL,SW_SHOWDEFAULT);
#endif
#ifdef TUVOK_OS_APPLE
  system("start "TUTORIAL_URL);
#endif
#ifdef TUVOK_OS_LINUX
  system("firefox "TUTORIAL_URL); // TODO: Tom: instead of hoping for firefox to be installed integrate this into the UI
#endif
}

void MainWindow::CloseWelcome() {
  if (m_pWelcomeDialog->ShowAtStartup() != m_bShowWelcomeScreen) {
    QSettings settings;
    settings.setValue("UI/ShowWelcomeScreen", m_pWelcomeDialog->ShowAtStartup());
  }
}

