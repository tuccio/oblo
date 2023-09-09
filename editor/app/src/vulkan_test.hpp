#pragma once

#include <QFileDialog>
#include <QLCDNumber>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QVulkanFunctions>
#include <QVulkanWindow>

QT_FORWARD_DECLARE_CLASS(QTabWidget)
QT_FORWARD_DECLARE_CLASS(QPlainTextEdit)
QT_FORWARD_DECLARE_CLASS(QLCDNumber)

class VulkanRenderer : public QVulkanWindowRenderer
{
public:
    VulkanRenderer(QVulkanWindow* w);

    void initResources() override;
    void startNextFrame() override;

private:
    QVulkanWindow* m_window;
};

class VulkanWindow : public QVulkanWindow
{
    Q_OBJECT

public:
    QVulkanWindowRenderer* createRenderer() override;

signals:
    void vulkanInfoReceived(const QString& text);
    void frameQueued(int colorValue);
};

class VulkanTest : public QWidget
{
    Q_OBJECT

public:
    explicit VulkanTest(VulkanWindow* w, QPlainTextEdit* logWidget);

public slots:
    void onVulkanInfoReceived(const QString& text);
    void onFrameQueued(int colorValue);
    void onGrabRequested();

private:
    VulkanWindow* m_window;
    QTabWidget* m_infoTab;
    QPlainTextEdit* m_info;
    QLCDNumber* m_number;
};