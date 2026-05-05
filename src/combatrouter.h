#pragma once

#include <QObject>
#include <QString>
#include <cstdint>
#include <cstddef>

class SpawnShell;
class Spells;

// Parses OP_Action2 packets into structured combat events for the
// websocket layer. Sits at the daemon level (one instance), so the
// id→name and spellId→spellName lookups happen once per packet rather
// than once per connected client.
class CombatRouter : public QObject {
    Q_OBJECT
public:
    CombatRouter(SpawnShell* spawnShell, Spells* spells,
                 QObject* parent = nullptr);

public slots:
    // Wired to OP_Action2 by DaemonApp. Layout matches struct
    // action2Struct in everquest.h:2042.
    void action2(const uint8_t* data, size_t len, uint8_t dir);

public:
    // Stage A+5 — gate from --rust-opcodes OP_Action2
    void setUseRustAction2(bool on) { m_useRustAction2 = on; }

signals:
    void combatEvent(uint32_t sourceId, const QString& sourceName,
                     uint32_t targetId, const QString& targetName,
                     uint32_t type, int32_t damage,
                     uint32_t spellId, const QString& spellName);

private:
    SpawnShell* m_spawnShell;
    Spells*     m_spells;
    bool        m_useRustAction2 = false;
};
