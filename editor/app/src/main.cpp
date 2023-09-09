#include <QApplication>
#include <QLibraryInfo>
#include <QStyleFactory>
#include <QtPlugin>

#include "empty.hpp"
#include "vulkan_test.hpp"

Q_LOGGING_CATEGORY(lcVk, "qt.vulkan")

static QPointer<QPlainTextEdit> messageLogWidget;
static QtMessageHandler oldMessageHandler = nullptr;

static void messageHandler(QtMsgType msgType, const QMessageLogContext& logContext, const QString& text)
{
    if (!messageLogWidget.isNull())
        messageLogWidget->appendPlainText(text);
    if (oldMessageHandler)
        oldMessageHandler(msgType, logContext, text);
}

int main(int argc, char* argv[])
{
    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);

    QApplication a(argc, argv);

    qApp->setStyle(QStyleFactory::create("fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(15, 15, 15));
    palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);

    palette.setColor(QPalette::Highlight, QColor(142, 45, 197).lighter());
    palette.setColor(QPalette::HighlightedText, Qt::black);
    qApp->setPalette(palette);

    // MainForm m;
    // m.show();

    messageLogWidget = new QPlainTextEdit(QLatin1String(QLibraryInfo::build()) + QLatin1Char('\n'));
    messageLogWidget->setReadOnly(true);

    oldMessageHandler = qInstallMessageHandler(messageHandler);

    QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

    QVulkanInstance inst;
    inst.setLayers({"VK_LAYER_KHRONOS_validation"});

    if (!inst.create())
        qFatal("Failed to create Vulkan instance: %d", inst.errorCode());

    VulkanWindow* vulkanWindow = new VulkanWindow;
    vulkanWindow->setVulkanInstance(&inst);

    VulkanTest m(vulkanWindow, messageLogWidget.data());
    QObject::connect(vulkanWindow, &VulkanWindow::vulkanInfoReceived, &m, &VulkanTest::onVulkanInfoReceived);
    QObject::connect(vulkanWindow, &VulkanWindow::frameQueued, &m, &VulkanTest::onFrameQueued);

    m.resize(1024, 768);
    m.show();

    return a.exec();
}