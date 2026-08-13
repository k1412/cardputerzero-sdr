// SPDX-License-Identifier: MIT

#include "translations.h"

#include <array>

namespace i18n {
namespace {

using Catalog = std::array<const char*, static_cast<size_t>(Text::Count)>;

constexpr std::array<LocaleInfo, kLocaleCount> kLocales = {{
    {Locale::English, "en", "English", "inter-regular.ttf"},
    {Locale::SimplifiedChinese, "zh-CN", "简体中文", "noto-cjk-sc-ui.ttf"},
    {Locale::TraditionalChinese, "zh-TW", "繁體中文", "noto-cjk-tc-ui.ttf"},
    {Locale::Spanish, "es", "Español", "inter-regular.ttf"},
    {Locale::Japanese, "ja", "日本語", "noto-cjk-jp-ui.ttf"},
    {Locale::Korean, "ko", "한국어", "noto-cjk-kr-ui.ttf"},
    {Locale::French, "fr", "Français", "inter-regular.ttf"},
    {Locale::German, "de", "Deutsch", "inter-regular.ttf"},
    {Locale::PortugueseBrazil, "pt-BR", "Português", "inter-regular.ttf"},
    {Locale::Russian, "ru", "Русский", "noto-ui.ttf"},
}};

constexpr std::array<Catalog, kLocaleCount> kCatalogs = {{
    {"Zero SDR", "DEMO", "CONNECTING", "LIVE", "NO DEVICE", "USB ACCESS", "REPLUG USB", "DEVICE BUSY", "CLOSE OTHER SDR", "DEVICE ERROR", "AUTO GAIN", "MUTED", "NO AUDIO", "STEP", "TUNE", "SETTINGS", "LANGUAGE", "THEME", "GAIN", "AUDIO", "DARK", "LIGHT", "ON", "SOURCE", "BACK", "SELECT", "MENU", "MOVE", "CHANGE"},
    {"Zero SDR", "演示", "连接中", "实时", "未连接设备", "USB权限", "重新插拔USB", "设备占用", "关闭其他SDR", "设备错误", "自动增益", "静音", "无音频", "步进", "调谐", "设置", "语言", "主题", "增益", "音频", "深色", "浅色", "开", "信号源", "返回", "选择", "菜单", "移动", "更改"},
    {"Zero SDR", "示範", "連線中", "即時", "未連接裝置", "USB權限", "重新插拔USB", "裝置占用", "關閉其他SDR", "裝置錯誤", "自動增益", "靜音", "無音訊", "步進", "調諧", "設定", "語言", "主題", "增益", "音訊", "深色", "淺色", "開", "訊號源", "返回", "選擇", "選單", "移動", "更改"},
    {"Zero SDR", "DEMO", "CONECTANDO", "EN VIVO", "SIN DISPOSITIVO", "ACCESO USB", "RECONECTA USB", "OCUPADO", "CIERRA OTRO SDR", "ERROR DEL EQUIPO", "GANANCIA AUTO", "SILENCIO", "SIN AUDIO", "PASO", "SINTONIZAR", "AJUSTES", "IDIOMA", "TEMA", "GANANCIA", "AUDIO", "OSCURO", "CLARO", "SÍ", "FUENTE", "VOLVER", "ELEGIR", "MENÚ", "MOVER", "CAMBIAR"},
    {"Zero SDR", "デモ", "接続中", "受信中", "機器なし", "USB権限", "USBを再接続", "使用中", "他のSDRを終了", "機器エラー", "自動ゲイン", "ミュート", "音声なし", "ステップ", "選局", "設定", "言語", "テーマ", "ゲイン", "音声", "ダーク", "ライト", "オン", "入力", "戻る", "選択", "メニュー", "移動", "変更"},
    {"Zero SDR", "데모", "연결 중", "수신 중", "장치 없음", "USB 권한", "USB 다시 연결", "사용 중", "다른 SDR 종료", "장치 오류", "자동 게인", "음소거", "오디오 없음", "간격", "주파수", "설정", "언어", "테마", "게인", "오디오", "어둡게", "밝게", "켜짐", "소스", "뒤로", "선택", "메뉴", "이동", "변경"},
    {"Zero SDR", "DÉMO", "CONNEXION", "DIRECT", "AUCUN APPAREIL", "ACCÈS USB", "REBRANCHEZ USB", "OCCUPÉ", "FERMER AUTRE SDR", "ERREUR APPAREIL", "GAIN AUTO", "MUET", "SANS AUDIO", "PAS", "RÉGLER", "RÉGLAGES", "LANGUE", "THÈME", "GAIN", "AUDIO", "SOMBRE", "CLAIR", "OUI", "SOURCE", "RETOUR", "CHOISIR", "MENU", "BOUGER", "MODIFIER"},
    {"Zero SDR", "DEMO", "VERBINDUNG", "LIVE", "KEIN GERÄT", "USB-ZUGRIFF", "USB NEU STECKEN", "BELEGT", "ANDERE SDR SCHLIESS.", "GERÄTEFEHLER", "AUTO-PEGEL", "STUMM", "KEIN AUDIO", "SCHRITT", "ABSTIMMEN", "EINSTELLUNGEN", "SPRACHE", "DESIGN", "PEGEL", "AUDIO", "DUNKEL", "HELL", "AN", "QUELLE", "ZURÜCK", "WÄHLEN", "MENÜ", "BEWEGEN", "ÄNDERN"},
    {"Zero SDR", "DEMO", "CONECTANDO", "AO VIVO", "SEM DISPOSITIVO", "ACESSO USB", "RECONECTE USB", "OCUPADO", "FECHE OUTRO SDR", "ERRO NO DISPOSITIVO", "GANHO AUTO", "MUDO", "SEM ÁUDIO", "PASSO", "SINTONIZAR", "AJUSTES", "IDIOMA", "TEMA", "GANHO", "ÁUDIO", "ESCURO", "CLARO", "LIGADO", "FONTE", "VOLTAR", "ESCOLHER", "MENU", "MOVER", "ALTERAR"},
    {"Zero SDR", "ДЕМО", "ПОДКЛЮЧЕНИЕ", "ЭФИР", "НЕТ УСТРОЙСТВА", "ДОСТУП USB", "ПЕРЕПОДКЛ. USB", "ЗАНЯТО", "ЗАКРОЙТЕ ДРУГОЕ SDR", "ОШИБКА УСТРОЙСТВА", "АВТОУСИЛЕНИЕ", "БЕЗ ЗВУКА", "НЕТ ЗВУКА", "ШАГ", "НАСТРОЙКА", "ПАРАМЕТРЫ", "ЯЗЫК", "ТЕМА", "УСИЛЕНИЕ", "ЗВУК", "ТЁМНАЯ", "СВЕТЛАЯ", "ВКЛ", "ИСТОЧНИК", "НАЗАД", "ВЫБРАТЬ", "МЕНЮ", "ХОД", "СМЕНА"},
}};

size_t locale_index(Locale locale) {
    const auto index = static_cast<size_t>(locale);
    return index < kLocales.size() ? index : 0;
}

} // namespace

const std::array<LocaleInfo, kLocaleCount>& locales() {
    return kLocales;
}

const LocaleInfo& locale_info(Locale locale) {
    return kLocales[locale_index(locale)];
}

const char* translate(Locale locale, Text text) {
    const auto text_index = static_cast<size_t>(text);
    if (text_index >= static_cast<size_t>(Text::Count)) {
        return "";
    }
    return kCatalogs[locale_index(locale)][text_index];
}

} // namespace i18n
