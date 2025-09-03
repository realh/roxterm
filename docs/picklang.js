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

function redirectToCurrentLang() {
    let lang = getLang();
    let availableLangs = getAvailableLangs();
    lang = matchLang(lang, availableLangs);
    if (lang)
        redirectToLang(lang);
}

window.onload = redirectToCurrentLang;
