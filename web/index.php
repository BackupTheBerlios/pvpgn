<?php
require_once('config.php');
$dateformat = 'l F j, Y G:i:s';

function microtime_float()
{
	list($usec, $sec) = explode(" ", microtime());
	return ((float)$usec + (float)$sec);
	srand($usec);
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
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
  <title><?php echo $title; ?></title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<link href="pvpgn.css" rel="stylesheet" type="text/css">
</head>
<body>
<table width="100%" border="0" cellspacing="0" cellpadding="0">
  <tr> 
    <td><table width="100%" border="0" cellspacing="0" cellpadding="0">
        <tr> 
          <td><table width="100%" border="0" cellspacing="0" cellpadding="0">
              <tr>
                <td class="topbar">
					<div> 
                    <table width="100%" border="0" cellspacing="0" cellpadding="0">
                      <tr>
							<td style="height:50px"> <div>
                            <table width="100%" border="0" cellspacing="1">
                              <tr>
								<?php
								$linkwidth = mysql_fetch_row($temp = mysql_query("SELECT (100 / COUNT(*))+0 AS linkwidth FROM linklist"));
								mysql_free_result($temp);
								unset($temp);
								$query = mysql_query("SELECT url,target,name FROM linklist ORDER BY `order` ASC");
								while ($row = mysql_fetch_row($query)) {
								// target=\"".$row[1]."\"
	                                echo "								<td style=\"width:". round($linkwidth[0]) ."%\"><div style=\"text-align:center\"><a href=\"".$row[0]."\" class=\"link\">".$row[2]."</a></div></td>\n";
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
          <td style="vertical-align:top"> <div style="text-align:center"> </div>
            <div style="text-align:center"> 
              <table width="100%" border="0" cellspacing="0" cellpadding="0">
                <tr> 
                  <td style="vertical-align:top; width:1%">
                  <div style="text-align:left">
                  <?php
				  // Display the random image on the left
                  echo "                    <img src=\"images/left" . sprintf("%02d",rand(1,4)) . ".jpg\" alt=\"\" />\n";
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
					<table width="95%">
					  <tr>
						<td colspan="4" class="text13">
						  <p><strong>Support files</strong></p>
						</td>
					  </tr>
					  <tr>
						<td style="width:4%">&nbsp;						  
						</td>
						<td style="width:40%">
						  <table border="0" cellspacing="0" cellpadding="0">
							<tr>
							  <td>
								<a href="http://download.berlios.de/pvpgn/pvpgn-support-1.0.tar.gz" class="link">pvpgn-support-1.0.tar.gz</a>
							  </td>
							</tr>
						  </table>
						</td>
						<td class="text12" style="width:40%">
						  Support files
						</td>
						<td class="text12" style="width:16%">
						  123.09 KB
						</td>
					  </tr>
					  <tr>
						<td style="width:4%">&nbsp;
						  
						</td>
						<td style="width:40%">
						  <table border="0" cellspacing="0" cellpadding="0">
							<tr>
							  <td>
								<a href="http://download.berlios.de/pvpgn/pvpgn-support-1.0.zip" class="link">pvpgn-support-1.0.zip</a>
							  </td>
							</tr>
						  </table>
						</td>
						<td class="text12" style="width:40%">
						  Support files
						</td>
						<td class="text12" style="width:16%">
						   125.11 KB
						</td>
					  </tr>
					</table><br />
					  <table width="100%">
						<tr>
						  <td style="text-align:center">
							<a href="http://developer.berlios.de/project/showfiles.php?group_id=2291" class="link2">More files...</a>
						  </td>
						</tr>
					  </table>
					  <br />
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
          <td class="botbar">
			  <table style="width:100%" cellspacing="0" cellpadding="0">
			  	<tr>
			  	<td style="width:99%">
			  	&nbsp;
			  	</td>
				  <td>
					  <table cellspacing="0" cellpadding="0">
						<tr>
						  <td class="hostedby">
							Hosted
						  </td>
						</tr>
						<tr>
						  <td class="hostedby">
							by
						  </td>
						</tr>
					  </table>
				  </td>
				  <td style="width:1%">
					<a href="http://developer.berlios.de/" class="popup">
					  <img src="http://developer.berlios.de/bslogo.php?group_id=2291&amp;type=1" style="width:124px;height:32px;border:0"alt="BerliOS">
					</a>
				  </td>
				</tr>
			  </table>
		  </td>
        </tr>
      </table></td>
  </tr>
</table>
<div class="footer">
<?php
$footer = mysql_fetch_row($temp = mysql_query("SELECT value FROM config WHERE `key` = 'footer'"));
echo " ".$footer[0]."\n";
mysql_free_result($temp);
?>
</div>
<div class="footer">
<?php
$time = microtime_float() - $time_start;
echo "  Page generation time: ".sprintf("%2.2f",$time)." seconds\n";
?>
</div>
</body>
</html>
