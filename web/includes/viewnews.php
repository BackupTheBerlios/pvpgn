<?php
$query = mysql_query("SELECT t1.username,t1.email,t2.timestamp,t2.subject,t2.text FROM news_users AS t1, news_posts AS t2 WHERE t1.uid = t2.poster ORDER BY t2.timestamp DESC LIMIT 0,5",$dbh);

if ($row = mysql_fetch_row($query)) {
	do {
		echo "				  <table width=\"100%\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\">";
		echo "				    <tr>";
		echo "				      <td align=\"left\" class=\"text2\"><font face=\"Arial, Helvetica, sans-serif\">";
		echo "				        <strong>" . $row[3]. "</strong>";
		echo "				        <br><span class=\"smText\">Posted by <a href=\"mailto:". $row[1] ."\" class=\"newsmenu\">". $row[0] ."</a> on ".date($dateformat,$row[2])."</span><br /><br />";
		echo "				        <span class=\"text\">" . str_replace("\n","\n<br />\n",$row[4]) . "</span>";
		echo "				      </font></td></tr></table>";
		echo "				  <div>&nbsp;</div>";
		echo "				  <hr><div>&nbsp;</div>";
	} while ($row = mysql_fetch_row($query));
	mysql_free_result($query);
} else {
	echo "				  <div align=\"center\">No news is good news</div>\n";
}
?>
