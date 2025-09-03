function getAvailableLangs() {
    let links = document.getElementsByTagName('a');
    let langs = [];
    for (let link of links) {
        let l = link.href.split('/');
        let lang = l[l.length - 2];
        langs.push(longLang(lang));
    }
    console.log("getAvailableLangs: " + langs.join(', '));
    return langs;
}

function pickLang() {
    let lang = getLang();
    let langs = getAvailableLangs();
    for (let l of langs) {
        if (l === lang) {
            console.log(`pickLang: '${lang}' matched`);
            return l;
        }
    }
    lang = shortLang(lang);
    for (let l of langs) {
        if (shortLang(l) === lang)
            console.log(`pickLang: '${lang}' matched shortLang('${l}')`);
            return l;
    }
    return null;
}

function redirectToCurrentLang() {
    let lang = pickLang();
    if (lang)
        redirectToLang(lang);
}

window.onload = redirectToCurrentLang;
