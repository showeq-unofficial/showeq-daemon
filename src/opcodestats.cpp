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
//
// expectedDir filters out the obvious wrong-direction false positives —
// e.g. OP_HPUpdate is overwhelmingly server→client (server tells client
// when an HP value changes); a 33-packet client→server opcode at the
// right size is almost certainly something else (target update, ping,
// etc.). All current hints are S>C dominant.
struct StructHint {
    const char* opcodeName;
    const char* structName;
    int         size;
    uint8_t     expectedDir;   // DIR_Server typically
};
// DIR_Server / DIR_Client come from packetcommon.h.
const StructHint kHints[] = {
    {"OP_Stamina",      "staminaStruct",        8, DIR_Server},
    {"OP_HPUpdate",     "hpNpcUpdateStruct",   18, DIR_Server},
    {"OP_ManaChange",   "manaDecrementStruct", 20, DIR_Server},
    {"OP_Action2",      "action2Struct",       48, DIR_Server},
    {"OP_Buff",         "buffStruct",         168, DIR_Server},
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
    // unresolved opcode, score every unknown zone opcode by:
    //   (1) does the dominant direction match the expected direction?
    //   (2) how many packets at the matching size went in that direction?
    // Strong matches (right direction + many packets at the right size)
    // bubble up. Wrong-direction candidates are demoted but still listed
    // so the user can spot edge cases. False positives still possible.
    out << "# candidate matches (unresolved opcodes vs known struct sizes)\n";
    out << "# Take with salt - same size doesn't prove same struct.\n";
    out << "# Each candidate row: opcode  matching-dir-count  total-at-size  expected/actual-dir\n";
    for (const auto& hint : kHints) {
        out << QString("# %1 (%2, %3 bytes, expected %4):")
                   .arg(hint.opcodeName, hint.structName)
                   .arg(hint.size)
                   .arg(dirLabel(hint.expectedDir)) << "\n";

        struct Candidate {
            uint16_t op;
            int      sizeCountTotal;   // packets of any dir at this size
            int      sizeCountInDir;   // packets at this size in expected dir
            QString  dominantDirs;     // human-readable summary
        };
        std::vector<Candidate> matches;
        for (auto it = m_zone.begin(); it != m_zone.end(); ++it) {
            if (!it->name.isEmpty()) continue;
            if (!it->sizeCounts.contains(hint.size)) continue;
            // Direction info per opcode is aggregated across all sizes
            // (we don't record dir-per-size). For known-struct-only
            // opcodes that's accurate; multi-purpose opcodes will be
            // less precise but still informative.
            const int sizeTotal = it->sizeCounts[hint.size];
            const int dirCount = it->dirCounts.value(hint.expectedDir, 0);
            // Approximate "size + expected dir" overlap by min(size, dir).
            // Underestimates if other-sized packets in the expected dir
            // exist; overestimates if they don't. Good enough to rank.
            const int approxOverlap = std::min(sizeTotal, dirCount);
            QString dirs;
            for (auto dit = it->dirCounts.begin(); dit != it->dirCounts.end(); ++dit) {
                if (!dirs.isEmpty()) dirs += ' ';
                dirs += QString("%1=%2").arg(dirLabel(dit.key())).arg(dit.value());
            }
            matches.push_back({it.key(), sizeTotal, approxOverlap, dirs});
        }
        // Sort: right-direction matches first, then by overlap desc,
        // then by total size-count desc.
        std::sort(matches.begin(), matches.end(),
                  [](const Candidate& a, const Candidate& b) {
                      if ((a.sizeCountInDir > 0) != (b.sizeCountInDir > 0))
                          return a.sizeCountInDir > b.sizeCountInDir;
                      if (a.sizeCountInDir != b.sizeCountInDir)
                          return a.sizeCountInDir > b.sizeCountInDir;
                      return a.sizeCountTotal > b.sizeCountTotal;
                  });
        if (matches.empty()) {
            out << "#   (no unknown opcodes seen with this size)\n\n";
            continue;
        }
        // Cap output at top 5 to keep the report readable.
        const int rows = std::min<int>(5, matches.size());
        for (int i = 0; i < rows; ++i) {
            const auto& c = matches[i];
            const bool dirOK = c.sizeCountInDir > 0;
            out << QString("#   0x%1  size@%2=%3  dir-match~%4  dirs(%5)%6")
                       .arg(c.op, 4, 16, QChar('0'))
                       .arg(hint.size)
                       .arg(c.sizeCountTotal)
                       .arg(c.sizeCountInDir)
                       .arg(c.dominantDirs)
                       .arg(dirOK ? "" : "  [WRONG DIR]")
                << "\n";
        }
        if ((int)matches.size() > rows) {
            out << QString("#   ... %1 more, omitted").arg(matches.size() - rows) << "\n";
        }
        out << "\n";
    }
}
