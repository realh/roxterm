<?php
/* index.php for ROXTerm. Adds a language selection menu to the online version
 * of the web pages.
 * Copyright (c) 2010 Tony Houghton <h@realh.co.uk>.
 * Licence: GPL v3 or later.
 * This script is for uploading to the project web pages,
 * not for distribution with the package.
 */
function make_filename($lang, $page)
{
    return str_replace('index.php', "$lang/$page.html",
            $_SERVER['SCRIPT_FILENAME']);
}

if (isset($_GET['page']))
{
    $page = $_GET['page'];
}
else
{
    $page = 'index';
}
if (isset($_GET['lang']))
{
    $lang = $_GET['lang'];
    setcookie('roxterm_lang', $lang);
}
elseif (isset($_COOKIE['roxterm_lang']))
{
    $lang = $_COOKIE['roxterm_lang'];
}
else
{
    $lang = 'en';
}
switch ($page)
{
    case 'guide':
    case 'index':
    case 'installation':
    case 'news':
        break;
    default:
        $page = 'index';
}
switch ($lang)
{
    case 'en':
    case 'pt_BR':
    case 'fr':
    case 'es':
    case 'ru':
    case 'uk':
        break;
    default:
        $lang = 'en';
}
$filename = make_filename($lang, $page);
$file = fopen($filename, 'r');
if (!$file)
{
    /* Redirect should cause a generic 404 */
    header('Location: http://' . $_SERVER['HTTP_HOST'] . '/' .
            dirname($_SERVER['PHP_SELF']) . "/$lang/$page.html");
    exit();
}
while (!feof($file))
{
    $line = fgets($file);
    $line = str_replace('../lib/' , 'lib/', $line);
    $line = preg_replace('/href="(guide|index|installation|news)\.html"/',
            "href=\"index.php?page=\\1&lang=$lang\"", $line);
    if (trim($line) == '<!-- PHP PLACEHOLDER -->')
    {
        print '<form id="LangForm" method="GET" action="http://' .
                $_SERVER['HTTP_HOST'] . '/' . $_SERVER['PHP_SELF'] . '">';
        print '<select name="lang" onChange="javascript:this.form.submit()">';
        print '<option value="en"';
        if ($lang == 'en')
        {
            print ' selected="1"';
        }
        print '>English</option>';
        print '<option value="pt_BR"';
        if ($lang == 'pt_BR')
        {
            print ' selected="1"';
        }
        print '>Português (Brasil)</option>';
        print '<option value="es"';
        if ($lang == 'es')
        {
            print ' selected="1"';
        }
        print '>Español</option>';
        print '<option value="fr"';
        if ($lang == 'fr')
        {
            print ' selected="1"';
        }
        print '>Français</option>';
        print '<option value="ru"';
        if ($lang == 'ru')
        {
            print ' selected="1"';
        }
        print '>Русский</option>';
        print '<option value="uk"';
        if ($lang == 'uk')
        {
            print ' selected="1"';
        }
        print '>Український</option>';
        print '</select>';
        print '<noscript><button id="LangButton">&#187;</button></noscript>';
        print '<input type="hidden" name="page" value="' . $page . '"/>';
        print "</form>\n";
    }
    else
    {
        print $line;
    }
}
fclose($file);
?>
