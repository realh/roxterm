const availableLangs = {
    en: 'English',
    es: 'Español',
    fr: 'Français',
    pt_BR: 'Português (Brasil)',
    ru: 'Русский',
    uk: 'Український',
}

// VPage or "virtual page" refers to which of Welcome, News, Installation or
// Guide should be viewed.
function getCurrentVPage() {
    const urlParams = new URLSearchParams(window.location.search);
    let vPage = urlParams.has('p') ? urlParams.get('p') : 'Welcome';
    if (!['Welcome', 'News', 'Installation', 'Guide'].includes(vPage))
        vPage = 'Welcome';
    return vPage;
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
    const currentPath = window.location.pathname.split('/');
    const currentPage = currentPath[currentPath.length - 1];
    window.location.href = `../${lang}/${currentPage}?p=${getCurrentVPage()}`;
}

function langOfCurrentPage() {
    let currentPath = window.location.pathname.split('/');
    return currentPath[currentPath.length - 2];
}

function addLangMenu() {
    let currentLang = langOfCurrentPage();
    currentLang = matchLang(currentLang, Object.keys(availableLangs));
    let menuHtml = buildMenu(currentLang);
    let div = document.getElementById('lang-selector');
    div.innerHTML = menuHtml;
}

function modifyLinks() {
    const links = document.querySelectorAll('#Header li a');
    for (let link of links) {
        let href = link.href;
        if (href.indexOf('#') === -1) {
            href = 'Welcome';
        } else {
            href = href.split('#')[1];
            if (href === '') {
                href = 'Welcome';
            }
        }
        href = 'index.html?p=' + href;
        console.log(`Modifying link ${link.href} to ${href}`);
        link.href = href;
    }
}

function selectVPage(vPage) {
    console.log(`selectVPage(${vPage})`);
    const links = document.querySelectorAll('#Header li a');
    for (let link of links) {
        if (link.href.endsWith(vPage)) {
            console.log(`Adding active class to link ${link.href}`);
            link.className = 'active';
        } else {
            console.log(`Removing active class from link ${link.href}`);
            link.className = '';
        }
    }
    const divs = document.getElementsByClassName('Middle');
    for (let div of divs) {
        if (div.id === vPage) {
            div.className = 'Middle';
        } else {
            div.className = 'Middle invisible';
        }
    }
}

function init() {
    modifyLinks();
    selectVPage(getCurrentVPage());
    addLangMenu();
}

window.onload = init;
