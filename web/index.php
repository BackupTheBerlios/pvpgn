<?php
require_once('config.php');
$dateformat = 'l F j, Y G:i:s';

function microtime_float()
{
	list($usec, $sec) = explode(" ", microtime());
	return ((float)$usec + (float)$sec);
}
$time_start = microtime_float();

$dbh = mysql_connect($dbhost,$dbuser,$dbpass);
mysql_select_db($dbname,$dbh);

if (!isset($_GET['page'])) {
	$_GET['page'] = 'index';
}

$query = mysql_query("SELECT title FROM titles WHERE `page` = '".$_GET['page']."';",$dbh);
if ($row = mysql_fetch_row($query)) {
	$title = $row[0];
	mysql_free_result($query);
	unset($query);
	unset($row);
} else {
	die('Hacking attempt!');
}
?>
<html>
<head>
  <title><?php echo $title; ?></title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<style>
body{
background-color:#000000;
background-repeat: repeat-x;
scrollbar-base-color:333333;
}
A.menulink {
display: block;
width: 125px;
text-align: center;
text-decoration: none;
font-family:arial;
font-weight:bold;
font-size:12px;
color: #FFB019;
BORDER: none;
}
A.menulink:hover {
background-color:#333333;
}
A.downloadlink {
display: block;
text-align: left;
text-decoration: none;
font-family:arial;
font-weight:bold;
font-size:12px;
color: #FFB019;
BORDER: none;
}
A.downloadlink:hover {
background-color:#333333;
}
.text {
font-family: Arial, Helvetica, sans-serif;
font-size: 13px;
color: #FFFFFF
}
.helptitle {
font-family: Arial, Helvetica, sans-serif;
font-size: 18px;
color: #FFB019
}
a.text2 {
font-family: Arial, Helvetica, sans-serif;
font-size: 12px;
color: #FFB019;
text-decoration: underline;
}
.text3 {
font-family: Arial, Helvetica, sans-serif;
font-size: 12px;
color: #FFFFFF
}
.smText {font-family: Arial, Helvetica, sans-serif;
font-size: 9px;
color: #FFFFFF
}
.text2 {
font-family: Arial, Helvetica, sans-serif;
font-size: 13px;
color: #FFB019
}
a.newsmenu {
font-family: Arial, Helvetica, sans-serif;
font-size: 9px;
color: #FFB019;
text-decoration: none;
}
a.newsmenu:hover {
font-family: Arial, Helvetica, sans-serif;
font-size: 9px;
color: #FFB019
}
a.newsmenu:visited {
font-family: Arial, Helvetica, sans-serif;
font-size: 9px;
color: #FFB019
}
a.newsmenu:visited:hover {
font-family: Arial, Helvetica, sans-serif;
font-size: 9px;
color: #FFB019
}
</style>
<body bgcolor="#000000" text="#FFFFFF" link="#FFB019" vlink="#FFB019" alink="#FFB019" leftmargin="0" topmargin="0" marginwidth="0" marginheight="0" rightmargin="0">
<table width="100%" border="0" cellspacing="0" cellpadding="0">
  <tr> 
    <td><table width="100%" border="0" cellspacing="0" cellpadding="0">
        <tr> 
          <td><table width="100%" border="0" cellspacing="0" cellpadding="0">
              <tr>
                <td height="68" valign="bottom" background="images/topbar-bg.gif">
				  <div align="center"> 
                    <table width="90%" border="0" cellspacing="0" cellpadding="0">
                      <tr> 
                        <td height="50"> <div align="center"> 
                            <table width="750" border="0" cellspacing="1">
                              <tr>
								<?php
								$linkwidth = mysql_fetch_row($temp = mysql_query("SELECT (100 / COUNT(*))+0 AS linkwidth FROM linklist"));
								mysql_free_result($temp);
								unset($temp);
								$query = mysql_query("SELECT url,target,name FROM linklist ORDER BY `order` ASC");
								while ($row = mysql_fetch_row($query)) {
	                                echo "								<td width=\"". round($linkwidth[0]) ."%\"><div align=\"center\"><a href=\"".$row[0]."\" target=\"".$row[1]."\" class=\"menulink\">".$row[2]."</a></div></td>\n";
								}
								mysql_free_result($query);
								unset($query);
								unset($row);
								unset($linkwidth);
								?>
                              </tr>
                            </table>
                          </div></td>
                      </tr>
                    </table>
                  </div></td>
              </tr>
            </table></td>
        </tr>
        <tr> 
          <td valign="top"> <div align="center"> </div>
            <div align="center"> 
              <table width="100%" border="0" cellspacing="0" cellpadding="0">
                <tr> 
                  <td valign="top" width="1%">
                  <div align="left">
                  <?php
				  // Display the random image on the left
                  echo "                    <img src=\"images/left" . sprintf("%02d",rand(1,4)) . ".jpg\">\n";
                  ?>
                  </div>
				  </td>
				  <td valign="top" align="left">
				  <?php
				  if ($_GET['page'] == 'files') {
				// This is how we process the files page
				// We start by getting the latest version numbers from the "config" table
				// Then we'll save these version numbers into variables and free the result
				  $row = mysql_fetch_row($temp = mysql_query("SELECT value FROM config WHERE `key` = 'latest_stable'",$dbh));
				  $latest_stable = $row[0];
				  mysql_free_result($temp);
				  $row = mysql_fetch_row($temp = mysql_query("SELECT value FROM config WHERE `key` = 'latest_unstable'",$dbh));
				  $latest_unstable = $row[0];
				  mysql_free_result($temp);
				  unset($temp);
				  unset($row);

				// A function to format the size field.  Taken from PHP-Nuke
				  function formatsize($size) {
    				$mb = 1024*1024;
    				if ( $size > $mb ) {
    				    $mysize = sprintf ("%01.2f",$size/$mb) . " MB";
    				} elseif ( $size >= 1024 ) {
    				    $mysize = sprintf ("%01.2f",$size/1024) . " KB";
    				} else {
    				    $mysize = $size . " bytes";
    				}
						return $mysize;
				  }

				  echo "<br />\n";

				// Check to see if there is an unstable release currently
				  if ($latest_unstable <> 0) {
					// Pass some instructions to files.php and run it
				  	$filesphp = array(
									'type' => 'unstable',
									'version' => $latest_unstable);
					include('files.php');
				  }
				// We will assume that there will always be a stable release
				  $filesphp = array(
								'type' => 'stable',
								'version' => $latest_stable);
				  include('files.php');

				// Show downloads for support files.  There is no reason to keep these in a database
				// cos they rarely change.
				  ?>
				  <p>
					<table width="95%">
					  <tr>
						<td colspan="4" class="text">
						  <p><strong>Support files</strong></p>
						</td>
					  </tr>
					  <tr>
						<td colspan="4">
						</td>
					  </tr>
					  <tr>
						<td width="4%">&nbsp;
						  
						</td>
						<td width="40%">
						  <table border="0" cellspacing="0" cellpadding="0">
							<tr>
							  <td>
								<a href="http://download.berlios.de/pvpgn/pvpgn-support-1.0.tar.gz" class="downloadlink">pvpgn-support-1.0.tar.gz</a>
							  </td>
							</tr>
						  </table>
						</td>
						<td class="text3" width="40%">
						  Support files
						</td>
						<td class="text3" width="16%">
						  <?php echo formatsize(126047); ?>
						</td>
					  </tr>
					  <tr>
						<td width="4%">&nbsp;
						  
						</td>
						<td width="40%">
						  <table border="0" cellspacing="0" cellpadding="0">
							<tr>
							  <td>
								<a href="http://download.berlios.de/pvpgn/pvpgn-support-1.0.zip" class="downloadlink">pvpgn-support-1.0.zip</a>
							  </td>
							</tr>
						  </table>
						</td>
						<td class="text3" width="40%">
						  Support files
						</td>
						<td class="text3" width="16%">
						  <?php echo formatsize(128117); ?>
						</td>
					  </tr>
					</table>
				    <p align="center">
					  <table align="center">
						<tr>
						  <td>
							<a href="http://developer.berlios.de/project/showfiles.php?group_id=2291" class="downloadlink">More files...</a>
						  </td>
						</tr>
					  </table>
					</p>
				  <?php
				  // End files page
				  } else if ($_GET['page'] == 'about') {
					// "What is PvPGN?" page
				  	readfile('includes/about.htm');
				  } else if ($_GET['page'] == 'help') {
					// "Help" page
					echo "<div>&nbsp;</div>\n";
					readfile('includes/help.htm');
				  } else {
				 // To show the news page, we just include viewnews.php
				 	echo "<div>&nbsp;</div>";
					include('includes/viewnews.php');
				  }
				  ?>
				  </td>
                </tr>
              </table>
            </div></td>
        </tr>
        <tr> 
          <td height="63" valign="top" background="images/botbar-bg.gif"> 
            <div align="center">
			  <table border="0" cellspacing="0" cellpadding="0">
			  	<tr>
				  <td>
				    <div align="right">
					  <table border="0" cellspacing="0" cellpadding="0">
						<tr>
						  <td class="smText" align="right">
							Hosted
						  </td>
						</tr>
						<tr>
						  <td class="smText" align="right">
							by
						  </td>
						</tr>
					  </table>
					</div>
				  </td>
				  <td width="1%">
					<a href="http://developer.berlios.de">
					  <img src="http://developer.berlios.de/bslogo.php?group_id=2291&type=1" width="124" height="32" border="0" alt="BerliOS">
					</a>
				  </td>

				</tr>
			  </table>
			</div>
		  </td>
        </tr>
      </table></td>
  </tr>
</table>
<div align="center"><font color="#666666" size="1" face="Verdana, Arial, Helvetica, sans-serif">
<?php
$footer = mysql_fetch_row($temp = mysql_query("SELECT value FROM config WHERE `key` = 'footer'"));
echo " ".$footer[0]."\n";
mysql_free_result($temp);
?>
</font></div>
<div align="center"><font color="#666666" size="1" face="Verdana, Arial, Helvetica, sans-serif">
<?php
$time = microtime_float() - $time_start;
echo "  Page generation time: ".sprintf("%2.2f",$time)." seconds\n";
?>
</font></div>
</body>
</html>
