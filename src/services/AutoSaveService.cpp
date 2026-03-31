/**
 * @file AutoSaveService.cpp
 * @brief 自动保存服务实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/services/AutoSaveService.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QtConcurrent>
#include <QDebug>

namespace EnhanceVision {

AutoSaveService* AutoSaveService::s_instance = nullptr;

AutoSaveService::AutoSaveService(QObject* parent)
    : QObject(parent)
{
}

AutoSaveService* AutoSaveService::instance()
{
    if (!s_instance) {
        s_instance = new AutoSaveService();
    }
    return s_instance;
}

void AutoSaveService::initialize()
{
    qInfo() << "[AutoSaveService] Initialized";
}

bool AutoSaveService::isAutoSaveEnabled() const
{
    return SettingsController::instance()->autoSaveResult();
}

QString AutoSaveService::getDefaultSavePath() const
{
    return SettingsController::instance()->defaultSavePath();
}

void AutoSaveService::autoSaveResult(const QString& taskId, const QString& resultPath)
{
    if (!isAutoSaveEnabled()) {
        qInfo() << "[AutoSaveService] Auto-save is disabled, skipping";
        return;
    }
    
    QString savePath = getDefaultSavePath();
    if (savePath.isEmpty()) {
        qInfo() << "[AutoSaveService] Default save path is empty, skipping";
        return;
    }
    
    if (resultPath.isEmpty()) {
        qInfo() << "[AutoSaveService] Result path is empty, skipping";
        return;
    }
    
    QFileInfo srcInfo(resultPath);
    if (!srcInfo.exists()) {
        qWarning() << "[AutoSaveService] Source file does not exist:" << resultPath;
        emit autoSaveCompleted(taskId, false, QString(), tr("源文件不存在"));
        return;
    }
    
    {
        QMutexLocker locker(&m_mutex);
        if (m_pendingSaves.contains(taskId)) {
            qWarning() << "[AutoSaveService] Task already has pending save:" << taskId;
            return;
        }
        m_pendingSaves[taskId] = true;
    }
    
    QString destPath = generateUniquePath(savePath, resultPath);
    
    qInfo() << "[AutoSaveService] Starting async save from:" << resultPath << "to:" << destPath;
    
    QtConcurrent::run([this, taskId, resultPath, destPath, savePath]() {
        bool success = false;
        QString error;
        
        if (!ensureDirectoryExists(savePath)) {
            error = tr("无法创建目标目录: %1").arg(savePath);
            qWarning() << "[AutoSaveService] Failed to create directory:" << savePath;
        } else {
            if (QFile::copy(resultPath, destPath)) {
                success = true;
                qInfo() << "[AutoSaveService] Successfully auto-saved:" << resultPath << "to:" << destPath;
            } else {
                error = tr("文件复制失败");
                qWarning() << "[AutoSaveService] Failed to copy:" << resultPath << "to:" << destPath;
            }
        }
        
        {
            QMutexLocker locker(&m_mutex);
            m_pendingSaves.remove(taskId);
        }
        
        emit autoSaveCompleted(taskId, success, success ? destPath : QString(), error);
    });
}

QString AutoSaveService::generateUniquePath(const QString& targetDir, const QString& originalPath)
{
    QFileInfo srcInfo(originalPath);
    QString baseName = srcInfo.completeBaseName();
    QString suffix = srcInfo.suffix();
    QString fileName = srcInfo.fileName();
    
    QString destPath = targetDir + "/" + fileName;
    
    int counter = 1;
    while (QFile::exists(destPath)) {
        destPath = QString("%1/%2_%3.%4")
            .arg(targetDir)
            .arg(baseName)
            .arg(counter++)
            .arg(suffix);
    }
    
    return destPath;
}

bool AutoSaveService::ensureDirectoryExists(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        if (dir.mkpath(".")) {
            qInfo() << "[AutoSaveService] Created directory:" << path;
            return true;
        } else {
            qWarning() << "[AutoSaveService] Failed to create directory:" << path;
            return false;
        }
    }
    return true;
}

} // namespace EnhanceVision
