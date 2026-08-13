// SPDX-License-Identifier: MIT

#include "translations.h"

#include <cassert>
#include <cstring>
#include <set>
#include <string>

int main() {
    const auto& locales = i18n::locales();
    assert(locales.size() == i18n::kLocaleCount);

    std::set<std::string> codes;
    for (const auto& locale : locales) {
        assert(locale.code != nullptr && locale.code[0] != '\0');
        assert(locale.native_name != nullptr && locale.native_name[0] != '\0');
        assert(locale.font_asset != nullptr && locale.font_asset[0] != '\0');
        assert(codes.emplace(locale.code).second);

        for (size_t text = 0; text < static_cast<size_t>(i18n::Text::Count); ++text) {
            const auto* translated = i18n::translate(locale.locale, static_cast<i18n::Text>(text));
            assert(translated != nullptr && std::strlen(translated) > 0);
        }
    }

    assert(std::strcmp(i18n::translate(i18n::Locale::SimplifiedChinese, i18n::Text::Settings), "设置") == 0);
    assert(std::strcmp(i18n::translate(i18n::Locale::Japanese, i18n::Text::Language), "言語") == 0);
}
