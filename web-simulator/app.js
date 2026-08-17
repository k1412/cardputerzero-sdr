"use strict";

const LOCALES = [
  { code: "en", name: "English" },
  { code: "zh-CN", name: "简体中文" },
  { code: "zh-TW", name: "繁體中文" },
  { code: "es", name: "Español" },
  { code: "ja", name: "日本語" },
  { code: "ko", name: "한국어" },
  { code: "fr", name: "Français" },
  { code: "de", name: "Deutsch" },
  { code: "pt-BR", name: "Português" },
  { code: "ru", name: "Русский" },
];

const TEXT = {
  en: ["DEMO", "CONNECTING", "LIVE", "NO DEVICE", "USB ACCESS", "REPLUG USB", "DEVICE BUSY", "CLOSE OTHER SDR", "DEVICE ERROR", "AUTO GAIN", "MUTED", "NO AUDIO", "STEP", "TUNE", "SETTINGS", "LANGUAGE", "THEME", "GAIN", "AUDIO", "DARK", "LIGHT", "ON", "BACK", "SELECT", "MENU", "MOVE", "CHANGE"],
  "zh-CN": ["演示", "连接中", "实时", "未连接设备", "USB权限", "重新插拔USB", "设备占用", "关闭其他SDR", "设备错误", "自动增益", "静音", "无音频", "步进", "调谐", "设置", "语言", "主题", "增益", "音频", "深色", "浅色", "开", "返回", "选择", "菜单", "移动", "更改"],
  "zh-TW": ["示範", "連線中", "即時", "未連接裝置", "USB權限", "重新插拔USB", "裝置占用", "關閉其他SDR", "裝置錯誤", "自動增益", "靜音", "無音訊", "步進", "調諧", "設定", "語言", "主題", "增益", "音訊", "深色", "淺色", "開", "返回", "選擇", "選單", "移動", "更改"],
  es: ["DEMO", "CONECTANDO", "EN VIVO", "SIN DISPOSITIVO", "ACCESO USB", "RECONECTA USB", "OCUPADO", "CIERRA OTRO SDR", "ERROR DEL EQUIPO", "GANANCIA AUTO", "SILENCIO", "SIN AUDIO", "PASO", "SINTONIZAR", "AJUSTES", "IDIOMA", "TEMA", "GANANCIA", "AUDIO", "OSCURO", "CLARO", "SÍ", "VOLVER", "ELEGIR", "MENÚ", "MOVER", "CAMBIAR"],
  ja: ["デモ", "接続中", "受信中", "機器なし", "USB権限", "USBを再接続", "使用中", "他のSDRを終了", "機器エラー", "自動ゲイン", "ミュート", "音声なし", "ステップ", "選局", "設定", "言語", "テーマ", "ゲイン", "音声", "ダーク", "ライト", "オン", "戻る", "選択", "メニュー", "移動", "変更"],
  ko: ["데모", "연결 중", "수신 중", "장치 없음", "USB 권한", "USB 다시 연결", "사용 중", "다른 SDR 종료", "장치 오류", "자동 게인", "음소거", "오디오 없음", "간격", "주파수", "설정", "언어", "테마", "게인", "오디오", "어둡게", "밝게", "켜짐", "뒤로", "선택", "메뉴", "이동", "변경"],
  fr: ["DÉMO", "CONNEXION", "DIRECT", "AUCUN APPAREIL", "ACCÈS USB", "REBRANCHEZ USB", "OCCUPÉ", "FERMER AUTRE SDR", "ERREUR APPAREIL", "GAIN AUTO", "MUET", "SANS AUDIO", "PAS", "RÉGLER", "RÉGLAGES", "LANGUE", "THÈME", "GAIN", "AUDIO", "SOMBRE", "CLAIR", "OUI", "RETOUR", "CHOISIR", "MENU", "BOUGER", "MODIFIER"],
  de: ["DEMO", "VERBINDUNG", "LIVE", "KEIN GERÄT", "USB-ZUGRIFF", "USB NEU STECKEN", "BELEGT", "ANDERE SDR SCHLIESS.", "GERÄTEFEHLER", "AUTO-PEGEL", "STUMM", "KEIN AUDIO", "SCHRITT", "ABSTIMMEN", "EINSTELLUNGEN", "SPRACHE", "DESIGN", "PEGEL", "AUDIO", "DUNKEL", "HELL", "AN", "ZURÜCK", "WÄHLEN", "MENÜ", "BEWEGEN", "ÄNDERN"],
  "pt-BR": ["DEMO", "CONECTANDO", "AO VIVO", "SEM DISPOSITIVO", "ACESSO USB", "RECONECTE USB", "OCUPADO", "FECHE OUTRO SDR", "ERRO NO DISPOSITIVO", "GANHO AUTO", "MUDO", "SEM ÁUDIO", "PASSO", "SINTONIZAR", "AJUSTES", "IDIOMA", "TEMA", "GANHO", "ÁUDIO", "ESCURO", "CLARO", "LIGADO", "VOLTAR", "ESCOLHER", "MENU", "MOVER", "ALTERAR"],
  ru: ["ДЕМО", "ПОДКЛЮЧЕНИЕ", "ЭФИР", "НЕТ УСТРОЙСТВА", "ДОСТУП USB", "ПЕРЕПОДКЛ. USB", "ЗАНЯТО", "ЗАКРОЙТЕ ДРУГОЕ SDR", "ОШИБКА УСТРОЙСТВА", "АВТОУСИЛЕНИЕ", "БЕЗ ЗВУКА", "НЕТ ЗВУКА", "ШАГ", "НАСТРОЙКА", "ПАРАМЕТРЫ", "ЯЗЫК", "ТЕМА", "УСИЛЕНИЕ", "ЗВУК", "ТЁМНАЯ", "СВЕТЛАЯ", "ВКЛ", "НАЗАД", "ВЫБРАТЬ", "МЕНЮ", "ХОД", "СМЕНА"],
};

const T = Object.freeze({ DEMO: 0, CONNECTING: 1, LIVE: 2, MISSING: 3, ACCESS: 4, REPLUG: 5, BUSY: 6, CLOSE: 7, ERROR: 8, AUTO: 9, MUTED: 10, NO_AUDIO: 11, STEP: 12, TUNE: 13, SETTINGS: 14, LANGUAGE: 15, THEME: 16, GAIN: 17, AUDIO: 18, DARK: 19, LIGHT: 20, ON: 21, BACK: 22, SELECT: 23, MENU: 24, MOVE: 25, CHANGE: 26 });
const STEPS = [10_000, 50_000, 100_000, 200_000, 500_000, 1_000_000];
const GAINS = [-99, -40, 71, 179, 192];
const LIMITS = { min: 22_000_000, max: 948_600_000 };
const DEFAULT_STATE = { frequencyHz: 97_400_000, stepIndex: 3, autoGain: true, gain: 192, muted: false, dark: true, page: "radio", locale: "zh-CN", scenario: "device", settingsRow: 0, direct: "", directInvalid: false };

const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => Array.from(document.querySelectorAll(selector));
const screen = $("#device-screen");
const canvas = $("#spectrum");
const ctx = canvas.getContext("2d");
let state = { ...DEFAULT_STATE };
let animationSeed = 0;
let toastTimer = 0;
let repeatDelay = 0;
let repeatTimer = 0;
let lastDrawnDeviceFrame = 0;
const hardware = {
  connected: false,
  status: "connecting",
  detail: "",
  deviceName: "",
  sampleRateHz: 0,
  frameSequence: 0,
  iqBytes: 0,
  iqBlocks: 0,
  readErrors: 0,
  spectrum: [],
};

function localText(index) { return TEXT[state.locale]?.[index] ?? TEXT.en[index]; }
function localeName() { return LOCALES.find((item) => item.code === state.locale)?.name ?? "English"; }
function formatFrequency() { return (state.frequencyHz / 1_000_000).toFixed(3); }
function formatStep() { return STEPS[state.stepIndex] >= 1_000_000 ? `${STEPS[state.stepIndex] / 1_000_000} MHz` : `${STEPS[state.stepIndex] / 1_000} kHz`; }
function formatGain() { return state.autoGain ? localText(T.AUTO) : `${(state.gain / 10).toFixed(1)} dB`; }

function scenarioPresentation() {
  switch (state.scenario) {
    case "device": {
      if (!hardware.connected || hardware.status === "connecting") return { source: localText(T.CONNECTING), audio: localText(T.ON), warning: "", active: false, device: true };
      if (hardware.status === "live") return { source: localText(T.LIVE), audio: state.muted ? localText(T.MUTED) : localText(T.ON), warning: "", active: true, device: true };
      if (hardware.status === "missing" || hardware.status === "library_missing") return { source: localText(T.MISSING), audio: localText(T.NO_AUDIO), warning: "", active: false, error: true, device: true };
      if (hardware.status === "access") return { source: localText(T.ACCESS), audio: localText(T.NO_AUDIO), warning: localText(T.REPLUG), active: false, error: true, device: true };
      if (hardware.status === "busy") return { source: localText(T.BUSY), audio: localText(T.NO_AUDIO), warning: localText(T.CLOSE), active: false, error: true, device: true };
      return { source: localText(T.ERROR), audio: localText(T.NO_AUDIO), warning: localText(T.ERROR), active: false, error: true, device: true };
    }
    case "live": return { source: localText(T.LIVE), audio: state.muted ? localText(T.MUTED) : localText(T.ON), warning: "", active: true };
    case "connecting": return { source: localText(T.CONNECTING), audio: localText(T.ON), warning: "", active: false };
    case "missing": return { source: localText(T.MISSING), audio: localText(T.NO_AUDIO), warning: "", active: false, error: true };
    case "access": return { source: localText(T.ACCESS), audio: localText(T.NO_AUDIO), warning: localText(T.REPLUG), active: false, error: true };
    case "busy": return { source: localText(T.BUSY), audio: localText(T.NO_AUDIO), warning: localText(T.CLOSE), active: false, error: true };
    case "error": return { source: localText(T.ERROR), audio: localText(T.NO_AUDIO), warning: localText(T.ERROR), active: false, error: true };
    case "noaudio": return { source: localText(T.LIVE), audio: localText(T.NO_AUDIO), warning: localText(T.NO_AUDIO), active: true, error: false };
    default: return { source: localText(T.DEMO), audio: state.muted ? localText(T.MUTED) : localText(T.ON), warning: "", active: true };
  }
}

function render() {
  const presentation = scenarioPresentation();
  screen.className = `screen ${state.dark ? "is-dark" : "is-light"} locale-${state.locale}`;
  $("#frequency").textContent = formatFrequency();
  $("#gain-label").textContent = formatGain();
  $("#step-label").textContent = formatStep();
  $("#source-pill").textContent = presentation.source;
  $("#source-pill").classList.toggle("is-error", Boolean(presentation.error));
  $("#audio-warning").textContent = presentation.warning;

  const directOpen = state.page === "direct";
  $("#radio-page").hidden = state.page === "settings";
  $("#settings-page").hidden = state.page !== "settings";
  $("#direct-panel").hidden = !directOpen;
  $("#direct-panel").classList.toggle("invalid", state.directInvalid);
  $("#direct-title").textContent = state.directInvalid ? (state.locale.startsWith("zh") ? "范围 22–948.6 MHz" : "RANGE 22–948.6 MHz") : (state.locale.startsWith("zh") ? "输入 MHz" : "TUNE MHz");
  $("#direct-value").textContent = `${state.direct || ""}_`;
  $("#direct-hint").textContent = `ENT ${localText(T.SELECT)} / ESC ${localText(T.BACK)}`;

  const settingLabels = [localText(T.LANGUAGE), localText(T.THEME), localText(T.GAIN), localText(T.AUDIO)];
  const settingValues = [localeName(), state.dark ? localText(T.DARK) : localText(T.LIGHT), formatGain(), presentation.audio];
  $$('[data-setting-label]').forEach((node, index) => { node.textContent = settingLabels[index]; });
  ["#setting-language", "#setting-theme", "#setting-gain", "#setting-audio"].forEach((selector, index) => { $(selector).textContent = settingValues[index]; });
  $$(".setting-row").forEach((row, index) => row.classList.toggle("selected", index === state.settingsRow));

  if (state.page === "settings") {
    $("#hint-left").textContent = `F/X ${localText(T.MOVE)}`;
    $("#hint-center").textContent = `Z/C ${localText(T.CHANGE)}`;
    $("#hint-right").textContent = `ENT ${localText(T.BACK)}`;
  } else {
    $("#hint-left").textContent = `F/X ${localText(T.STEP)}`;
    $("#hint-center").textContent = `Z/C ${localText(T.TUNE)}`;
    $("#hint-right").textContent = `ENT ${localText(T.MENU)}`;
  }

  $("#scenario").value = state.scenario;
  $("#locale").value = state.locale;
  $("#theme").value = state.dark ? "dark" : "light";
  $$('[data-page]').forEach((button) => button.classList.toggle("active", button.dataset.page === state.page));
  $("#readout-frequency").textContent = `${formatFrequency()} MHz`;
  $("#readout-step").textContent = formatStep();
  $("#readout-gain").textContent = formatGain();
  $("#readout-audio").textContent = presentation.audio;
  $("#readout-device").textContent = hardware.connected
    ? `${hardware.deviceName || "RTL-SDR"} · ${hardware.status === "live" ? "在线" : "异常"}`
    : "未连接";
  $("#readout-iq").textContent = formatBytes(hardware.iqBytes);
  const liveStatus = $("#global-live-status");
  liveStatus.classList.toggle("offline", !hardware.connected || hardware.status !== "live");
  liveStatus.lastChild.textContent = hardware.connected && hardware.status === "live" ? " 真机在线" : " 真机连接中";
  document.documentElement.style.colorScheme = state.dark ? "dark" : "light";
  saveState();
  replaceStateUrl();
}

function setEvent(message) {
  $("#event-message").textContent = message;
}

function tune(direction) {
  state.frequencyHz = Math.max(LIMITS.min, Math.min(LIMITS.max, state.frequencyHz + STEPS[state.stepIndex] * direction));
  setEvent(`${direction > 0 ? "向右" : "向左"}调谐至 ${formatFrequency()} MHz。`);
  sendDeviceControl({ frequency_hz: state.frequencyHz });
}

function cycleStep(direction) {
  state.stepIndex = (state.stepIndex + (direction > 0 ? 1 : STEPS.length - 1)) % STEPS.length;
  setEvent(`步进调整为 ${formatStep()}。`);
}

function adjustGain(direction) {
  if (state.autoGain) {
    state.autoGain = false;
    state.gain = direction < 0 ? GAINS.at(-1) : GAINS[0];
  } else {
    const current = GAINS.indexOf(state.gain);
    if (direction < 0 && current === 0 || direction > 0 && current === GAINS.length - 1) state.autoGain = true;
    else state.gain = GAINS[current + direction];
  }
  setEvent(`增益调整为 ${formatGain()}。`);
  sendDeviceControl({ automatic_gain: state.autoGain, gain_tenths_db: state.gain });
}

function adjustSetting(direction) {
  if (state.settingsRow === 0) cycleLocale(direction);
  if (state.settingsRow === 1) { state.dark = !state.dark; setEvent(`切换为${state.dark ? "深色" : "浅色"}主题。`); }
  if (state.settingsRow === 2) adjustGain(direction);
  if (state.settingsRow === 3) { state.muted = !state.muted; setEvent(state.muted ? "音频已静音。" : "音频已开启。"); }
}

function cycleLocale(direction = 1) {
  const current = LOCALES.findIndex((locale) => locale.code === state.locale);
  state.locale = LOCALES[(current + (direction > 0 ? 1 : LOCALES.length - 1)) % LOCALES.length].code;
  setEvent(`界面语言切换为 ${localeName()}。`);
}

function appendDirect(character) {
  if (state.page !== "direct") { state.page = "direct"; state.direct = ""; }
  const [whole = "", fraction] = state.direct.split(".");
  if (character === ".") {
    if (fraction !== undefined) return;
    state.direct = `${state.direct || "0"}.`;
  } else if (fraction !== undefined) {
    if (fraction.length < 3) state.direct += character;
  } else if (whole.length < 3) {
    state.direct += character;
  }
  state.directInvalid = false;
  setEvent(`正在直接输入频率：${state.direct || "—"} MHz。`);
}

function commitDirect() {
  const value = Number(state.direct);
  if (!state.direct || !Number.isFinite(value) || value < 22 || value > 948.6) {
    state.directInvalid = true;
    setEvent("频率无效，请输入 22–948.6 MHz。");
    return;
  }
  state.frequencyHz = Math.round(value * 1_000_000);
  state.page = "radio";
  state.direct = "";
  state.directInvalid = false;
  setEvent(`直接调谐完成：${formatFrequency()} MHz。`);
  sendDeviceControl({ frequency_hz: state.frequencyHz });
}

function handleAction(action) {
  const direct = state.page === "direct";
  if (/^\d$/.test(action)) appendDirect(action);
  else if (action === "decimal") appendDirect(".");
  else if (action === "backspace" && direct) { state.direct = state.direct.slice(0, -1); state.directInvalid = false; setEvent(`删除一位：${state.direct || "—"} MHz。`); }
  else if (action === "enter" && direct) commitDirect();
  else if (action === "escape" && direct) { state.page = "radio"; state.direct = ""; state.directInvalid = false; setEvent("已取消直接调谐，频率保持不变。"); }
  else if (action === "enter" && state.page === "radio") { state.page = "settings"; setEvent("进入设置页。"); }
  else if ((action === "enter" || action === "escape") && state.page === "settings") { state.page = "radio"; setEvent("返回无线电页。"); }
  else if (action === "escape") setEvent("在实体设备上，此操作将返回 APPLaunch。");
  else if (action === "left") state.page === "settings" ? adjustSetting(-1) : tune(-1);
  else if (action === "right") state.page === "settings" ? adjustSetting(1) : tune(1);
  else if (action === "up") state.page === "settings" ? (state.settingsRow = (state.settingsRow + 3) % 4, setEvent(`选择设置项 ${state.settingsRow + 1}/4。`)) : cycleStep(1);
  else if (action === "down") state.page === "settings" ? (state.settingsRow = (state.settingsRow + 1) % 4, setEvent(`选择设置项 ${state.settingsRow + 1}/4。`)) : cycleStep(-1);
  else if (action === "g") { state.autoGain = !state.autoGain; setEvent(`增益切换为 ${formatGain()}。`); sendDeviceControl({ automatic_gain: state.autoGain, gain_tenths_db: state.gain }); }
  else if (action === "m") { state.muted = !state.muted; setEvent(state.muted ? "音频已静音。" : "音频已开启。"); }
  else if (action === "l") cycleLocale(1);
  else if (action === "t") { state.dark = !state.dark; setEvent(`切换为${state.dark ? "深色" : "浅色"}主题。`); }
  render();
}

function keyFromEvent(event) {
  const map = { ArrowLeft: "left", ArrowRight: "right", ArrowUp: "up", ArrowDown: "down", Enter: "enter", Escape: "escape", Backspace: "backspace", Delete: "backspace", ".": "decimal", g: "g", G: "g", m: "m", M: "m", l: "l", L: "l", t: "t", T: "t" };
  return /^\d$/.test(event.key) ? event.key : map[event.key];
}

function flashKey(action, pressed) {
  const button = $(`[data-key="${action}"]`);
  if (button) button.classList.toggle("pressed", pressed);
}

function saveState() {
  const persistent = { frequencyHz: state.frequencyHz, stepIndex: state.stepIndex, autoGain: state.autoGain, gain: state.gain, muted: state.muted, dark: state.dark, locale: state.locale, scenario: state.scenario };
  localStorage.setItem("zero-sdr-simulator-state", JSON.stringify(persistent));
}

function loadState() {
  try { state = { ...state, ...JSON.parse(localStorage.getItem("zero-sdr-simulator-state") || "{}") }; } catch { /* Ignore invalid local state. */ }
  const params = new URLSearchParams(location.search);
  if (LOCALES.some((locale) => locale.code === params.get("locale"))) state.locale = params.get("locale");
  if (["dark", "light"].includes(params.get("theme"))) state.dark = params.get("theme") === "dark";
  if ($(`#scenario option[value="${CSS.escape(params.get("scenario") || "")}"]`)) state.scenario = params.get("scenario");
  if (["radio", "settings", "direct"].includes(params.get("page"))) state.page = params.get("page");
  const frequency = Number(params.get("frequency"));
  if (Number.isFinite(frequency) && frequency >= 22 && frequency <= 948.6) state.frequencyHz = Math.round(frequency * 1_000_000);
}

function replaceStateUrl() {
  const params = new URLSearchParams({ locale: state.locale, theme: state.dark ? "dark" : "light", scenario: state.scenario, page: state.page, frequency: formatFrequency() });
  history.replaceState(null, "", `${location.pathname}?${params}${location.hash}`);
}

function showToast(message) {
  const toast = $("#toast");
  toast.textContent = message;
  toast.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = window.setTimeout(() => toast.classList.remove("show"), 2200);
}

function formatBytes(bytes) {
  if (!Number.isFinite(bytes) || bytes <= 0) return "0 B";
  const units = ["B", "KB", "MB", "GB"];
  const index = Math.min(units.length - 1, Math.floor(Math.log(bytes) / Math.log(1024)));
  return `${(bytes / 1024 ** index).toFixed(index === 0 ? 0 : 1)} ${units[index]}`;
}

async function sendDeviceControl(fields) {
  if (state.scenario !== "device") return;
  try {
    const response = await fetch("/api/control", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(fields),
    });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
  } catch (error) {
    setEvent(`真机控制暂时失败：${error.message}`);
  }
}

async function pollHardware() {
  const previousStatus = hardware.status;
  const wasConnected = hardware.connected;
  try {
    const response = await fetch("/api/status", { cache: "no-store" });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const payload = await response.json();
    if (payload.schema !== "zero-sdr-bridge-v1" || !Array.isArray(payload.spectrum)) throw new Error("桥接协议不兼容");
    hardware.connected = true;
    hardware.status = payload.status;
    hardware.detail = payload.detail || "";
    hardware.deviceName = payload.device_name || "RTL-SDR";
    hardware.sampleRateHz = payload.sample_rate_hz || 0;
    hardware.frameSequence = payload.frame_sequence || 0;
    hardware.iqBytes = payload.iq_bytes || 0;
    hardware.iqBlocks = payload.iq_blocks || 0;
    hardware.readErrors = payload.read_errors || 0;
    hardware.spectrum = payload.spectrum.slice(0, 128);
    if (state.scenario === "device") {
      state.frequencyHz = payload.frequency_hz || state.frequencyHz;
      state.autoGain = Boolean(payload.automatic_gain);
      state.gain = Number.isFinite(payload.gain_tenths_db) ? payload.gain_tenths_db : state.gain;
    }
    if (!wasConnected || previousStatus !== hardware.status) {
      setEvent(hardware.status === "live"
        ? `真机已连接：${hardware.deviceName}，${(hardware.sampleRateHz / 1_000_000).toFixed(3)} MS/s。`
        : `真机状态：${hardware.detail || hardware.status}。`);
    }
  } catch (error) {
    hardware.connected = false;
    hardware.status = "connecting";
    hardware.detail = error.message;
    if (wasConnected) setEvent("真机桥已断开，正在自动重连。");
  }
  render();
  window.setTimeout(pollHardware, 350);
}

async function copyText(text, success) {
  try { await navigator.clipboard.writeText(text); showToast(success); }
  catch { window.prompt("复制下面的内容：", text); }
}

function drawSpectrum() {
  const dark = state.dark;
  const presentation = scenarioPresentation();
  const w = canvas.width;
  const h = canvas.height;
  const chartH = 100;
  ctx.fillStyle = dark ? "#06111b" : "#e9f0f2";
  ctx.fillRect(0, 0, w, chartH);
  ctx.strokeStyle = dark ? "#183247" : "#b8c9ce";
  ctx.lineWidth = 1;
  for (let x = 0; x <= w; x += w / 4) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, chartH); ctx.stroke(); }
  for (let y = 0; y <= chartH; y += chartH / 2) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke(); }

  const realDeviceFrame = state.scenario === "device" && hardware.connected && hardware.status === "live" && hardware.spectrum.length >= 64;
  const bins = realDeviceFrame ? hardware.spectrum.length : 96;
  const values = [];
  animationSeed += presentation.active ? .08 : .018;
  for (let i = 0; i < bins; i++) {
    if (realDeviceFrame) {
      values.push(Math.max(2, Math.min(98, hardware.spectrum[i])));
    } else {
      const x = i / (bins - 1);
      const noise = presentation.active ? Math.sin(i * 2.17 + animationSeed) * 4 + Math.sin(i * .61 - animationSeed * 2) * 3 : 0;
      const carrier = presentation.active ? Math.exp(-Math.pow((x - .51) * 16, 2)) * 62 : 0;
      const side = state.scenario === "live" || state.scenario === "noaudio" ? Math.exp(-Math.pow((x - .28) * 34, 2)) * 18 : 0;
      values.push(Math.max(4, 18 + noise + carrier + side));
    }
  }

  ctx.strokeStyle = dark ? "#35dcc8" : "#007f78";
  ctx.lineWidth = 3;
  ctx.beginPath();
  values.forEach((value, i) => {
    const x = i / (bins - 1) * w;
    const y = chartH - value;
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.stroke();

  if (realDeviceFrame) {
    if (hardware.frameSequence !== lastDrawnDeviceFrame) {
      const waterfallTop = chartH + 4;
      ctx.drawImage(canvas, 0, waterfallTop + 2, w, h - waterfallTop - 2, 0, waterfallTop, w, h - waterfallTop - 2);
      values.forEach((value, index) => {
        const ratio = value / 100;
        ctx.fillStyle = ratio > .7 ? "#ffd166" : ratio > .35 ? "#35dcc8" : ratio > .14 ? "#12395a" : "#03080d";
        const x = Math.floor(index / values.length * w);
        const next = Math.ceil((index + 1) / values.length * w);
        ctx.fillRect(x, h - 2, Math.max(1, next - x), 2);
      });
      lastDrawnDeviceFrame = hardware.frameSequence;
    }
  } else {
    const gradient = ctx.createLinearGradient(0, chartH, w, h);
    gradient.addColorStop(0, dark ? "#12395a" : "#b8d8db");
    gradient.addColorStop(.48, presentation.active ? (dark ? "#176f76" : "#65aaa4") : (dark ? "#07131c" : "#dce8ea"));
    gradient.addColorStop(.52, presentation.active ? "#35dcc8" : (dark ? "#0c1b26" : "#dfe9ec"));
    gradient.addColorStop(1, dark ? "#03080d" : "#f5f8f9");
    ctx.fillStyle = gradient;
    ctx.fillRect(0, chartH + 4, w, h - chartH - 4);
    if (presentation.active) {
      ctx.fillStyle = "rgba(255, 209, 102, .65)";
      ctx.fillRect(w * .485, chartH + 5, w * .045, h - chartH - 6);
    }
  }
}

function animate() {
  drawSpectrum();
  window.setTimeout(() => requestAnimationFrame(animate), 90);
}

function setup() {
  LOCALES.forEach(({ code, name }) => $("#locale").add(new Option(name, code)));
  loadState();
  render();

  document.addEventListener("keydown", (event) => {
    if (["TEXTAREA", "SELECT", "INPUT"].includes(document.activeElement?.tagName)) return;
    const action = keyFromEvent(event);
    if (!action) return;
    event.preventDefault();
    if (event.repeat && !["left", "right", "up", "down", "backspace"].includes(action)) return;
    flashKey(action, true);
    handleAction(action);
  });
  document.addEventListener("keyup", (event) => { const action = keyFromEvent(event); if (action) flashKey(action, false); });

  $$(".key, .pod-key").forEach((button) => {
    const stop = () => { clearTimeout(repeatDelay); clearInterval(repeatTimer); button.classList.remove("pressed"); };
    button.addEventListener("pointerdown", (event) => {
      event.preventDefault();
      button.setPointerCapture(event.pointerId);
      button.classList.add("pressed");
      handleAction(button.dataset.key);
      if (["left", "right", "up", "down", "backspace"].includes(button.dataset.key)) {
        repeatDelay = window.setTimeout(() => { repeatTimer = window.setInterval(() => handleAction(button.dataset.key), 110); }, 460);
      }
    });
    button.addEventListener("pointerup", stop);
    button.addEventListener("pointercancel", stop);
    button.addEventListener("lostpointercapture", stop);
  });

  $("#scenario").addEventListener("change", (event) => { state.scenario = event.target.value; setEvent(`接收场景切换为“${event.target.selectedOptions[0].text}”。`); if (state.scenario === "device" && hardware.connected) { state.frequencyHz = state.frequencyHz; sendDeviceControl({ frequency_hz: state.frequencyHz, automatic_gain: state.autoGain, gain_tenths_db: state.gain }); } render(); });
  $("#locale").addEventListener("change", (event) => { state.locale = event.target.value; setEvent(`界面语言切换为 ${localeName()}。`); render(); });
  $("#theme").addEventListener("change", (event) => { state.dark = event.target.value === "dark"; setEvent(`切换为${state.dark ? "深色" : "浅色"}主题。`); render(); });
  $$('[data-page]').forEach((button) => button.addEventListener("click", () => { state.page = button.dataset.page; if (state.page === "direct" && !state.direct) state.direct = "103.9"; state.directInvalid = false; setEvent(`切换到${button.textContent}页面。`); render(); }));
  $("#reset-button").addEventListener("click", () => { state = { ...DEFAULT_STATE }; setEvent("已恢复模拟器默认状态。"); render(); showToast("已恢复默认状态"); });
  $("#copy-link").addEventListener("click", () => copyText(location.href, "当前状态链接已复制"));

  const notes = $("#review-notes");
  notes.value = localStorage.getItem("zero-sdr-review-notes") || "";
  const savedChecks = new Set(JSON.parse(localStorage.getItem("zero-sdr-review-checks") || "[]"));
  $$(".checklist input").forEach((input) => { input.checked = savedChecks.has(input.value); input.addEventListener("change", saveReview); });
  notes.addEventListener("input", saveReview);
  $("#copy-report").addEventListener("click", () => {
    const checked = $$(".checklist input:checked").map((input) => `- [x] ${input.value}`);
    const pending = $$(".checklist input:not(:checked)").map((input) => `- [ ] ${input.value}`);
    const report = [`# Zero SDR 产品验收反馈`, ``, `- 状态：${$("#scenario").selectedOptions[0].text}`, `- 页面：${state.page}`, `- 频率：${formatFrequency()} MHz`, `- 步进：${formatStep()}`, `- 增益：${formatGain()}`, `- 语言：${localeName()}`, `- 主题：${state.dark ? "深色" : "浅色"}`, `- 复现链接：${location.href}`, ``, `## 验收清单`, ...checked, ...pending, ``, `## 改进意见`, notes.value.trim() || "（暂无）"].join("\n");
    copyText(report, "验收报告已复制");
  });

  screen.addEventListener("click", () => screen.focus());
  animate();
  pollHardware();
}

function saveReview() {
  localStorage.setItem("zero-sdr-review-notes", $("#review-notes").value);
  localStorage.setItem("zero-sdr-review-checks", JSON.stringify($$(".checklist input:checked").map((input) => input.value)));
  $("#save-status").textContent = "已自动保存";
}

setup();
