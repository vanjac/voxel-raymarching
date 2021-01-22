#include "opengllog.h"

static QDebug severityToDebug(QOpenGLDebugMessage::Severity severity);
static const char *sourceToString(QOpenGLDebugMessage::Source source);
static const char *typeToString(QOpenGLDebugMessage::Type type);

void logGLMessage(const QOpenGLDebugMessage &message)
{
    severityToDebug(message.severity()) << "OPENGL"
        << sourceToString(message.source()) << typeToString(message.type())
        << ":" << qPrintable(message.message().trimmed());
}

QDebug severityToDebug(QOpenGLDebugMessage::Severity severity)
{
    switch(severity) {
    case QOpenGLDebugMessage::Severity::HighSeverity:
        return qCritical();
    case QOpenGLDebugMessage::Severity::MediumSeverity:
        return qWarning();
    default:
        return qDebug();
    }
}

const char *sourceToString(QOpenGLDebugMessage::Source source)
{
    switch (source) {
    case QOpenGLDebugMessage::Source::APISource:
        return "API";
    case QOpenGLDebugMessage::Source::WindowSystemSource:
        return "WindowSystem";
    case QOpenGLDebugMessage::Source::ShaderCompilerSource:
        return "ShaderCompiler";
    case QOpenGLDebugMessage::Source::ThirdPartySource:
        return "ThirdParty";
    case QOpenGLDebugMessage::Source::ApplicationSource:
        return "Application";
    default:
        return "Unknown";
    }
}

const char *typeToString(QOpenGLDebugMessage::Type type)
{
    switch (type) {
    case QOpenGLDebugMessage::Type::ErrorType:
        return "Error";
    case QOpenGLDebugMessage::Type::DeprecatedBehaviorType:
        return "DeprecatedBehavior";
    case QOpenGLDebugMessage::Type::UndefinedBehaviorType:
        return "UndefinedBehavior";
    case QOpenGLDebugMessage::Type::PortabilityType:
        return "Portability";
    case QOpenGLDebugMessage::Type::PerformanceType:
        return "Performance";
    default:
        return "Unknown";
    }
}
