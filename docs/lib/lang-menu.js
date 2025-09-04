const availableLangs = {
    de: 'Deutsch',
    en: 'English',
    es: 'Español',
    fr: 'Français',
    gl: 'Galego',
    pt_BR: 'Português (Brasil)',
    ru: 'Русский',
    uk: 'Український',
}

function buildMenuItem(langCode, langName, matchedLang) {
    let s = `<option value="${langCode}"`;
    if (langCode === matchedLang) {
        s += ' selected="1"';
    }
    s += `>${langName}</option>`;
    return s;
}

function buildMenu(currentLang) {
    let s = '\n<select name="lang-select" ' +
        'onChange="onLangChanged(this.value)">\n';
    let langCodes = Object.keys(availableLangs).sort();
    for (let langCode of langCodes) {
        s += buildMenuItem(langCode, availableLangs[langCode], currentLang);
        s += '\n';
    }
    s += '</select>\n';
    return s;
}

function onLangChanged(lang) {
    if (lang === langOfCurrentPage())
        return;
    let currentPath = window.location.pathname.split('/');
    let currentPage = currentPath[currentPath.length - 1];
    window.location.href = `../${lang}/${currentPage}`;
}

function langOfCurrentPage() {
    let currentPath = window.location.pathname.split('/');
    return currentPath[currentPath.length - 2];
}

function init() {
    let currentLang = langOfCurrentPage();
    currentLang = matchLang(currentLang, Object.keys(availableLangs));
    console.log("currentLang: " + currentLang);
    let menuHtml = buildMenu(currentLang);
    console.log("menuHtml: " + menuHtml);
    let div = document.getElementById('lang-selector');
    div.innerHTML = menuHtml;
}

window.onload = init;
console.log("lang-menu.js loaded");
