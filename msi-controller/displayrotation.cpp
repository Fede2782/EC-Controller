#include "displayrotation.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace {
const QString kscreenDoctor = QStringLiteral("kscreen-doctor");

// KScreen::Output::Type::Panel - the built-in screen.
constexpr int panelOutputType = 7;

// KScreen::Output::Rotation values, as reported in "rotation".
constexpr int rotationNone = 1;
constexpr int rotationLeft = 2;
constexpr int rotationInverted = 4;
constexpr int rotationRight = 8;

// Only used on the blocking exit path; long enough for a slow compositor,
// short enough not to hold quitting up noticeably.
constexpr int exitTimeoutMs = 3000;

int halfTurn(int rotation)
{
    switch (rotation) {
    case rotationNone: return rotationInverted;
    case rotationInverted: return rotationNone;
    case rotationLeft: return rotationRight;
    case rotationRight: return rotationLeft;
    default: return rotationInverted;
    }
}

// The keyword kscreen-doctor's "output.<name>.rotation.<keyword>" expects.
QString rotationKeyword(int rotation)
{
    switch (rotation) {
    case rotationInverted: return QStringLiteral("inverted");
    case rotationLeft: return QStringLiteral("left");
    case rotationRight: return QStringLiteral("right");
    default: return QStringLiteral("none");
    }
}

QString program()
{
    return QStandardPaths::findExecutable(kscreenDoctor);
}

QStringList setRotationArguments(const QString &outputName, int rotation)
{
    return { QStringLiteral("output.%1.rotation.%2").arg(outputName, rotationKeyword(rotation)) };
}

QJsonArray activeOutputs(const QByteArray &json)
{
    QJsonArray active;
    const QJsonArray outputs = QJsonDocument::fromJson(json).object().value(QStringLiteral("outputs")).toArray();
    for (const QJsonValue &value : outputs) {
        const QJsonObject output = value.toObject();
        if (output.value(QStringLiteral("connected")).toBool() && output.value(QStringLiteral("enabled")).toBool()) {
            active.append(output);
        }
    }
    return active;
}
}

DisplayRotation::DisplayRotation(QObject *parent)
    : QObject(parent)
{
}

bool DisplayRotation::isAvailable()
{
    return !program().isEmpty();
}

std::optional<DisplayRotation::Output> DisplayRotation::findTarget(const QByteArray &json)
{
    const QJsonArray outputs = activeOutputs(json);

    // The built-in panel first: the key that triggers this is on the
    // laptop, so that's the screen it's about, even with an external one
    // set as primary.
    std::optional<QJsonObject> chosen;
    for (const QJsonValue &value : outputs) {
        if (value.toObject().value(QStringLiteral("type")).toInt() == panelOutputType) {
            chosen = value.toObject();
            break;
        }
    }

    // Otherwise the primary output: priority 1 is the primary, higher
    // numbers rank below it, 0 means unset.
    if (!chosen) {
        int bestPriority = 0;
        for (const QJsonValue &value : outputs) {
            const int priority = value.toObject().value(QStringLiteral("priority")).toInt();
            if (priority > 0 && (bestPriority == 0 || priority < bestPriority)) {
                bestPriority = priority;
                chosen = value.toObject();
            }
        }
    }

    if (!chosen && !outputs.isEmpty()) {
        chosen = outputs.first().toObject();
    }

    if (!chosen) {
        return std::nullopt;
    }
    return Output{ chosen->value(QStringLiteral("name")).toString(),
                   chosen->value(QStringLiteral("rotation")).toInt() };
}

std::optional<DisplayRotation::Output> DisplayRotation::findByName(const QByteArray &json, const QString &name)
{
    for (const QJsonValue &value : activeOutputs(json)) {
        const QJsonObject output = value.toObject();
        if (output.value(QStringLiteral("name")).toString() == name) {
            return Output{ name, output.value(QStringLiteral("rotation")).toInt() };
        }
    }
    return std::nullopt;
}

void DisplayRotation::flip()
{
    if (m_busy) {
        return;
    }

    const QString doctor = program();
    if (doctor.isEmpty()) {
        emit failed(tr("kscreen-doctor isn't installed."));
        return;
    }

    m_busy = true;

    // Step 1: read the current configuration.
    auto *query = new QProcess(this);
    connect(query, &QProcess::errorOccurred, this, [this, query](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            query->deleteLater();
            m_busy = false;
            emit failed(tr("kscreen-doctor couldn't be started."));
        }
    });
    connect(query, &QProcess::finished, this, [this, query, doctor](int exitCode, QProcess::ExitStatus status) {
        query->deleteLater();

        const std::optional<Output> output = (status == QProcess::NormalExit && exitCode == 0)
            ? findTarget(query->readAllStandardOutput())
            : std::nullopt;
        if (!output || output->name.isEmpty()) {
            m_busy = false;
            emit failed(tr("Couldn't find the display to rotate."));
            return;
        }

        if (!m_baselineRotation) {
            m_outputName = output->name;
            m_baselineRotation = output->rotation;
        }

        // Step 2: turn it half way round from wherever it is now.
        const int target = halfTurn(output->rotation);
        auto *apply = new QProcess(this);
        connect(apply, &QProcess::finished, this,
                [this, apply, target, name = output->name](int applyExitCode, QProcess::ExitStatus applyStatus) {
            apply->deleteLater();
            m_busy = false;
            if (applyStatus != QProcess::NormalExit || applyExitCode != 0) {
                emit failed(tr("kscreen-doctor couldn't rotate %1.").arg(name));
                return;
            }
            m_lastSetRotation = target;
        });
        apply->start(doctor, setRotationArguments(output->name, target));
    });
    query->start(doctor, { QStringLiteral("-j") });
}

void DisplayRotation::restoreBaseline()
{
    if (!m_baselineRotation || !m_lastSetRotation || *m_lastSetRotation == *m_baselineRotation) {
        return;
    }

    const QString doctor = program();
    if (doctor.isEmpty()) {
        return;
    }

    QProcess query;
    query.start(doctor, { QStringLiteral("-j") });
    if (!query.waitForFinished(exitTimeoutMs) || query.exitCode() != 0) {
        return;
    }

    // Only undo our own change: if the rotation is no longer what the last
    // flip set, someone changed it since, and that choice stands.
    const std::optional<Output> output = findByName(query.readAllStandardOutput(), m_outputName);
    if (!output || output->rotation != *m_lastSetRotation) {
        return;
    }

    QProcess apply;
    apply.start(doctor, setRotationArguments(m_outputName, *m_baselineRotation));
    apply.waitForFinished(exitTimeoutMs);
}
