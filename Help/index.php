<?php
/* index.php for ROXTerm. Adds a language selection menu to the online version
 * of the web pages.
 * Copyright (c) 2010 Tony Houghton <h@realh.co.uk>.
 * Licence: GPL v3 or later.
 * This script is for uploading to the project web pages,
 * not for distribution with the package.
 */
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
    setcookie('lang', $lang);
}
elseif (isset($_COOKIE['lang']))
{
    $lang = $_COOKIE['lang'];
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
    case 'es':
        break;
    default:
        $lang = 'en';
}
$filename = str_replace('index.php', "$lang/$page.html",
        $_SERVER['SCRIPT_FILENAME']);
$file = fopen($filename, 'r');
while (!feof($file))
{
    $line = fgets($file);
    $line = str_replace('../lib/' , 'lib/', $line);
    $line = preg_replace('/href="(guide|index|installation|news)\.html"/',
            "href=\"index.php?page=$page&lang=$lang\"", $line);
    if (trim($line) == '<!-- PHP PLACEHOLDER -->')
    {
        print '<div id="LangMenu">';
        print '<form method="GET" action="http://' . $_SERVER['HTTP_HOST'] .
                '/' . $_SERVER['PHP_SELF'] . '">';
        print '<select name="lang">';
        print '<option value="en"';
        if ($lang == 'en')
        {
            print ' selected="1"';
        }
        print '>English</option>';
        print '<option value="es"';
        if ($lang == 'es')
        {
            print ' selected="1"';
        }
        print '>Español</option>';
        print '</select>';
        print '<button>&#187;</button>';
        print '<hidden name="page" value="' . $page . '"/>';
        print "</form></div>\n";
    }
    else
    {
        print $line;
    }
}
fclose($file);
?>
