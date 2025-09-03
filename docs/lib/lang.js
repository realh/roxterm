// If '-' is present, it's replaced with '_'.
function longLang(l) {
    if (l.indexOf('/') !== -1)
        l = l.split('/')[0];
	l = l.replace('-',	'_');
	return l;
}

function getLang() {
    let lang = window.navigator.languages ? window.navigator.languages[0] : null;
    lang = lang || window.navigator.language || window.navigator.browserLanguage 
           || window.navigator.userLanguage;
    lang = longLang(lang);
    return lang;
}

// Strips off a lang variant if present, eg 'pt_BR' or 'pt-BR' becomes 'pt'
function shortLang(l) {
    if (l.indexOf('_') !== -1)
        return l.split('_')[0];
    if (l.indexOf('-') !== -1)
        return l.split('-')[0];
    return l;
}

function redirectToLang(lang) {
    window.location.href = lang + '/index.html';
}
