#include "prefsbroker.h"

#include <QString>

#include "main.h"
#include "xmlpreferences.h"

namespace {

// Allowlist v1 — a single chat-timestamp format string.
// Adding a new entry requires touching three places below:
//   1. fillSnapshot() — read current value into the proto
//   2. apply()        — match section/key/value-type and persist
//   3. (this comment) — keep the list of expected keys current
constexpr const char* kInterfaceSection      = "Interface";
constexpr const char* kDateTimeFormatKey     = "DateTimeFormat";
constexpr const char* kDateTimeFormatDefault = "ddd MMM dd,yyyy - hh:mm ap";

} // namespace

PrefsBroker::PrefsBroker(QObject* parent) : QObject(parent) {}

void PrefsBroker::fillSnapshot(seq::v1::PrefsSnapshot* out) const
{
    auto* p = out->add_prefs();
    p->set_section(kInterfaceSection);
    p->set_key(kDateTimeFormatKey);
    p->set_string_value(
        pSEQPrefs->getPrefString(kDateTimeFormatKey, kInterfaceSection,
                                 kDateTimeFormatDefault).toStdString());
}

bool PrefsBroker::apply(const seq::v1::Pref& pref)
{
    const QString section = QString::fromStdString(pref.section());
    const QString key     = QString::fromStdString(pref.key());

    if (section == kInterfaceSection && key == kDateTimeFormatKey) {
        if (pref.value_case() != seq::v1::Pref::kStringValue) return false;
        pSEQPrefs->setPrefString(
            key, section, QString::fromStdString(pref.string_value()));
        // XMLPreferences batches modifications; flush so the change
        // survives a daemon restart.
        pSEQPrefs->save();
        emit prefChanged(pref);
        return true;
    }

    return false;
}
