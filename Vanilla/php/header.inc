<?php
$host = $SERVER_NAME;
$path = '..';

if (is_file('conf.inc')) include('conf.inc');

$h1 = 'Netrek Server '.$host;
if ($fn != '') {
    $title = $title.' - '.$fn;
    $h2 = $fn;
} else {
    $title = $h1;
    $h2 = '';
}
?>
<html>
<head>
<title><?php echo $ti; ?></title>
</head>
<body vlink="#c6a832" link="#23db28" background="/img/stars3.gif"
 bgcolor="#000000" text="#aedbff">
<h1><?php echo $h1; ?></h1>
<?php
if ($h2 != '') echo '<h2>'.$h2.'</h2>';
?>
