<?php
require_once('../config.php');
$dateformat = 'l F j, Y G:i:s';
session_start();
?>
<html>
<head>
<title>Site admin</title>
</head>
<body bgcolor="#FFFFFF">
<?php
if (!session_is_registered('user') && $_GET['action'] != 'login') {
?>
<p align="center"><form action="<?php echo $_SERVER['PHP_SELF']; ?>?action=login" method="post">
Username: <input type="text" name="username" maxlength="32"><br />
Password: <input type="password" name="password"><br />
<input type="submit" value="Log in">
</form></p>
<?php
} else {
	if ($_GET['action'] == 'login' || $_GET['action'] == 'donewuser' || $_GET['action'] == 'dosubmit' || $_GET['action'] == 'downloads' || $_GET['action'] == 'dodownloads' || $_GET['action'] == 'edit' || $_GET['action'] == 'dodelete' || $_GET['action'] == 'doedit' || $_GET['action'] == 'edititem') {
		$dbh = mysql_connect($dbhost,$dbuser,$dbpass);
		mysql_select_db($dbname,$dbh);
	}
	if ($_GET['action'] == 'login') {
		$row = mysql_fetch_row(mysql_query("SELECT * FROM news_users WHERE username = '".$_POST['username']."';",$dbh));
		if (!$row) {
			echo "<p align=\"center\">User \"".$_POST['username']."\" does not exist</p>\n";
		} else if ($row[2] != md5($_POST['password'])) {
			echo "<p align=\"center\">Password incorrect</p>\n";
		} else {
			$_SESSION['uid'] = $row[0];
			$_SESSION['user'] = $row[1];
			$_SESSION['email'] = $row[3];
			echo "<p align=\"center\">Login successful</p>\n";
			echo "<p align=\"center\"><a href=\"".$_SERVER['PHP_SELF']."\">Click here to continue</a></p>\n";
		}
	} else {
		if ($_GET['action'] != 'logout') {
			?>
			<p align="center"><a href="<?php echo $_SERVER['PHP_SELF']; ?>?action=submit">Submit news</a> | <a href="<?php echo $_SERVER['PHP_SELF']; ?>?action=edit">Edit news</a> | <a href="<?php echo $_SERVER['PHP_SELF']; ?>?action=newuser">Create new user</a> | <a href="links.php">Edit links</a> | <a href="<?php echo $_SERVER['PHP_SELF']; ?>?action=downloads">Edit downloads</a> | <a href="<?php echo $_SERVER['PHP_SELF']; ?>?action=logout">Logout</a></p>
			<?php
		}
		if ($_GET['action'] == 'newuser') {
		?>
		<p><form action="<?php echo $_SERVER['PHP_SELF']; ?>?action=donewuser" method="post">
		Username: <input type="text" name="username" maxlength="32"><br />
		Password: <input type="password" name="pass1"><br />
		Confirm password: <input type="password" name="pass2"><br />
		Email address: <input type="text" name="email" maxlength="128"><br />
		<input type="submit" value="Create new user">
		</form></p>
		<?php
		} else if ($_GET['action'] == 'donewuser') {
			if ($_POST['pass1'] <> $_POST['pass2']) {
				echo "<p align=\"center\">Password and repeated password do not match</p>\n";
			} else {
				if (@mysql_query("INSERT INTO `news_users`(`username`,`passhash`,`email`) VALUES('".$_POST['username']."','".md5($_POST['pass1'])."','".$_POST['email']."');",$dbh)) {
					echo "<p align=\"center\">User \"".$_POST['username']."\" created successfully</p>\n";
				} else {
					echo "<p align=\"center\">Error: Could not create new user</p>\n";
					echo "<p align=\"center\">MySQL said: ".mysql_error()."</p>\n";
				}
			}
		} else if ($_GET['action'] == 'submit') {
			echo "<p><form action=\"".$_SERVER['PHP_SELF']."?action=dosubmit\" method=\"post\">\n";
			echo "<b>Username:</b> ".$_SESSION['user']."<br />\n";
			echo "<b>Email:</b> ".$_SESSION['email']."<br />\n";
			echo "<b>Date:</b> ".date($dateformat)."<br />\n";
		?>
		<b>Subject:</b> <input type="text" size="50" name="subject" maxlength="128"><br />
		<b>News text:</b><br />
		<textarea name="text" cols="50" rows="10"></textarea><br />
		<input type="submit" value="Submit news">
		</form></p>
		<?php
		} else if ($_GET['action'] == 'dosubmit') {
			if (@mysql_query("INSERT INTO `news_posts` VALUES(".time().",".$_SESSION['uid'].",'".$_POST['subject']."','".$_POST['text']."');",$dbh)) {
				echo "<p align=\"center\">News added successfully</p>\n";
			} else {
				echo "<p align=\"center\">Error: Could not insert news</p>\n";
				echo "<p align=\"center\">MySQL said: ".mysql_error()."</p>\n";		
			}
		} else if ($_GET['action'] == 'edit') {
			echo "<div align=\"center\"><table width=\"90%\">";
			echo "<tr bgcolor=\"#CCCCCC\"><td>Timestamp</td><td>Subject</td><td>Edit</td><td>Delete</td></tr>\n";
			$query = mysql_query("SELECT timestamp,subject FROM news_posts ORDER BY `timestamp` DESC",$dbh);
			if ($row = mysql_fetch_row($query)) {
				do {
					echo "<tr><td>".$row[0]."</td><td>".$row[1]."</td><td><a href=\"".$_SERVER['PHP_SELF']."?action=edititem&x=".$row[0]."\">edit</a></td><td><a href=\"".$_SERVER['PHP_SELF']."?action=dodelete&x=".$row[0]."\">delete</a></td>\n";
				} while ($row = mysql_fetch_row($query));
			} else {
				echo "<tr><td colspan=\"4\">No news is good news!</td></tr>\n";
			}
			echo "</table></div>\n";
		} else if ($_GET['action'] == 'dodelete') {
			mysql_query("DELETE FROM news_posts WHERE `timestamp` = ".$_GET['x'].";",$dbh);
			echo "<p align=\"center\">Newsitem deleted</p>\n";
		} else if ($_GET['action'] == 'edititem') {
			$row = mysql_fetch_row($temp = mysql_query("SELECT subject,text FROM news_posts WHERE `timestamp` = ".$_GET['x'].";",$dbh));
			echo "<p><form action=\"".$_SERVER['PHP_SELF']."?action=doedit&x=".$_GET['x']."\" method=\"post\">\n";
			echo "Date: ".date($dateformat,$_GET['x'])."<br />\n";
			echo "Subject: <input type=\"text\" name=\"subject\" value=\"".$row[0]."\"><br />\n";
			echo "<textarea name=\"text\" cols=\"50\" rows=\"10\">".$row[1]."</textarea><br />\n";
			echo "<input type=\"submit\" value=\"Update news\">\n";
			echo "</form></p>\n";
			mysql_free_result($temp);
			unset($temp);
			unset($row);
		} else if ($_GET['action'] == 'doedit') {
			if (@mysql_query("UPDATE news_posts SET `subject` = '".$_POST['subject']."', `text` = '".$_POST['text']."' WHERE `timestamp` = ".$_GET['x'].";",$dbh)) {
				echo "<p align=\"center\">News updated successfully</p>\n";
			} else {
				echo "<p align=\"center\">Error: Could not update news</p>\n";
				echo "<p align=\"center\">MySQL said: ".mysql_error()."</p>\n";		
			}			
		} else if ($_GET['action'] == 'downloads') {
			$row = mysql_fetch_row($temp = mysql_query("SELECT value FROM misc WHERE `key` = 'latest_stable'",$dbh));
			$latest_stable = $row[0];
			mysql_free_result($temp);
			$row = mysql_fetch_row($temp = mysql_query("SELECT value FROM misc WHERE `key` = 'latest_unstable'",$dbh));
			$latest_unstable = $row[0];
			mysql_free_result($temp);
			unset($temp);
			unset($row);
			echo "<form action=\"".$_SERVER['PHP_SELF']."?action=dodownloads\" method=\"post\">\n";
			echo "Latest unstable release: <input type=\"text\" name=\"unstable\" value=\"".$latest_unstable."\"> (enter 0 to disable)<br />\n";
			echo "<a href=\"downloads.php?type=unstable\">Edit files for unstable</a><br />\n<br />\n";
			echo "Latest stable release: <input type=\"text\" name=\"stable\" value=\"".$latest_stable."\"><br />\n";
			echo "<a href=\"downloads.php?type=stable\">Edit files for stable</a><br />\n<br />\n";
			echo "<input type=\"submit\" value=\"Apply changes\">\n";
			echo "</form>\n";
		} else if ($_GET['action'] == 'dodownloads') {
			if (mysql_query("UPDATE misc SET `value` = '".$_POST['unstable']."' WHERE `key` = 'latest_unstable';",$dbh) && mysql_query("UPDATE misc SET `value` = '".$_POST['stable']."' WHERE `key` = 'latest_stable';",$dbh)) {
				echo "<p align=\"center\">Updated successfully</p>\n";
			} else {
				echo "<p align=\"center\">Error: Could not apply changes</p>\n";
				echo "<p align=\"center\">MySQL said: ".mysql_error()."</p>\n";
			}
		} else if ($_GET['action'] == 'logout') {
			$_SESSION = array();
			if (isset($_COOKIE[session_name()])) {
				setcookie(session_name(), '', time()-42000, '/');
			}
			session_destroy();
			echo "<p align=\"center\">Logout successful</p>\n";
		}
	}
}
?>
</body>
</html>
