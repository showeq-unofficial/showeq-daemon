#include "opcodestats.h"

#include <QFile>
#include <QLoggingCategory>
#include <QTextStream>
#include <algorithm>
#include <vector>

#include "packet.h"
#include "packetcommon.h"
#include "packetinfo.h"

namespace {

// Known structs the daemon currently has handlers for but whose opcodes
// are still id="ffff" in conf/zoneopcodes.xml (or never resolved). The
// candidate-matching section of the report intersects payload sizes with
// these to suggest "OP_Foo is probably 0x????" pairings. Sizes from
// `sizeof(struct)` in everquest.h.
struct StructHint {
    const char* opcodeName;
    const char* structName;
    int         size;
};
const StructHint kHints[] = {
    {"OP_Stamina",      "staminaStruct",       8},
    {"OP_HPUpdate",     "hpNpcUpdateStruct",  18},
    {"OP_ManaChange",   "manaDecrementStruct", 20},
    {"OP_Action2",      "action2Struct",      48},
    {"OP_Buff",         "buffStruct",         168},
};

const char* dirLabel(uint8_t dir)
{
    switch (dir) {
        case DIR_Server: return "S>C";
        case DIR_Client: return "C>S";
        default:         return "?";
    }
}

QString formatSizeHist(const QHash<int,int>& sizes)
{
    std::vector<std::pair<int,int>> v;
    v.reserve(sizes.size());
    for (auto it = sizes.begin(); it != sizes.end(); ++it) {
        v.emplace_back(it.key(), it.value());
    }
    std::sort(v.begin(), v.end(),
              [](auto& a, auto& b) { return a.second > b.second; });
    QString out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ',';
        out += QString::number(v[i].first) + ':' + QString::number(v[i].second);
        if (i >= 5) { out += ",…"; break; }
    }
    return out;
}

QString formatDirs(const QHash<uint8_t,int>& dirs)
{
    QString out;
    bool first = true;
    for (auto it = dirs.begin(); it != dirs.end(); ++it) {
        if (!first) out += ' ';
        out += QString("%1 %2").arg(dirLabel(it.key())).arg(it.value());
        first = false;
    }
    return out;
}

} // namespace

OpcodeStatsLogger::OpcodeStatsLogger(EQPacket* packet, const QString& outPath,
                                     QObject* parent)
    : QObject(parent)
    , m_outPath(outPath)
{
    // The bool "unknown" overload is the one that fires; bind to it
    // explicitly so we don't pick up the 5-arg overload.
    connect(packet,
            SIGNAL(decodedZonePacket(const uint8_t*, size_t, uint8_t,
                                     uint16_t, const EQPacketOPCode*)),
            this,
            SLOT(onDecodedZonePacket(const uint8_t*, size_t, uint8_t,
                                     uint16_t, const EQPacketOPCode*)));
    connect(packet,
            SIGNAL(decodedWorldPacket(const uint8_t*, size_t, uint8_t,
                                      uint16_t, const EQPacketOPCode*)),
            this,
            SLOT(onDecodedWorldPacket(const uint8_t*, size_t, uint8_t,
                                      uint16_t, const EQPacketOPCode*)));
    qInfo("opcode-stats logging enabled, will write %s on shutdown",
          qUtf8Printable(outPath));
}

OpcodeStatsLogger::~OpcodeStatsLogger()
{
    writeReport();
}

void OpcodeStatsLogger::onDecodedZonePacket(const uint8_t*, size_t len,
                                            uint8_t dir, uint16_t opcode,
                                            const EQPacketOPCode* entry)
{
    record(m_zone, opcode, len, dir, entry);
}

void OpcodeStatsLogger::onDecodedWorldPacket(const uint8_t*, size_t len,
                                             uint8_t dir, uint16_t opcode,
                                             const EQPacketOPCode* entry)
{
    record(m_world, opcode, len, dir, entry);
}

void OpcodeStatsLogger::record(QHash<uint16_t, OpcodeStat>& bucket,
                               uint16_t opcode, size_t len, uint8_t dir,
                               const EQPacketOPCode* entry)
{
    OpcodeStat& s = bucket[opcode];
    if (s.name.isEmpty() && entry) s.name = entry->name();
    s.dirCounts[dir]++;
    s.sizeCounts[static_cast<int>(len)]++;
    s.total++;
}

void OpcodeStatsLogger::writeReport()
{
    if (m_written) return;
    m_written = true;

    QFile f(m_outPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("opcode-stats: cannot open %s for write: %s",
                 qUtf8Printable(m_outPath),
                 qUtf8Printable(f.errorString()));
        return;
    }
    QTextStream out(&f);

    auto dump = [&](const QHash<uint16_t, OpcodeStat>& bucket,
                    const char* label) {
        out << "# " << label << " opcodes (" << bucket.size()
            << " distinct, sorted by count desc)\n";
        out << "# columns: opcode  known/unknown  count  dirs  sizes  name\n";
        std::vector<std::pair<uint16_t, OpcodeStat>> v;
        v.reserve(bucket.size());
        for (auto it = bucket.begin(); it != bucket.end(); ++it) {
            v.emplace_back(it.key(), it.value());
        }
        std::sort(v.begin(), v.end(),
                  [](auto& a, auto& b) { return a.second.total > b.second.total; });
        for (const auto& [op, s] : v) {
            out << QString("0x%1").arg(op, 4, 16, QChar('0')) << "  ";
            out << (s.name.isEmpty() ? "unknown" : "known  ") << "  ";
            out << QString("%1").arg(s.total, 6) << "  ";
            out << formatDirs(s.dirCounts) << "  ";
            out << "sizes=" << formatSizeHist(s.sizeCounts);
            if (!s.name.isEmpty()) out << "  " << s.name;
            out << "\n";
        }
        out << "\n";
    };

    out << "# showeq-daemon opcode-stats report\n";
    out << "# Generated at process exit; covers everything EQPacket decoded\n";
    out << "# during this run. Use to spot id=\"ffff\" opcodes by matching\n";
    out << "# their payload sizes against known-struct sizes (see candidate\n";
    out << "# matches at the bottom).\n\n";

    dump(m_zone, "zone");
    dump(m_world, "world");

    // Candidate matches: for each known struct that maps to a still-
    // unresolved opcode, list every unknown zone opcode whose dominant
    // payload size matches. Heuristic — false positives possible — but
    // narrows the search space dramatically.
    out << "# candidate matches (unresolved opcodes vs known struct sizes)\n";
    out << "# Take with salt - same size doesn't prove same struct.\n";
    for (const auto& hint : kHints) {
        out << QString("# %1 (%2, %3 bytes):")
                   .arg(hint.opcodeName, hint.structName)
                   .arg(hint.size) << "\n";
        bool any = false;
        std::vector<std::pair<uint16_t, int>> matches;
        for (auto it = m_zone.begin(); it != m_zone.end(); ++it) {
            if (!it->name.isEmpty()) continue;  // only unknown opcodes
            if (it->sizeCounts.contains(hint.size)) {
                matches.emplace_back(it.key(), it->sizeCounts[hint.size]);
            }
        }
        std::sort(matches.begin(), matches.end(),
                  [](auto& a, auto& b) { return a.second > b.second; });
        for (const auto& [op, count] : matches) {
            out << QString("#   0x%1  (%2 packets at this size)")
                       .arg(op, 4, 16, QChar('0')).arg(count) << "\n";
            any = true;
        }
        if (!any) out << "#   (no unknown opcodes seen with this size)\n";
        out << "\n";
    }
}
