<?php
if (!isset($filesphp)) {
	die('Hacking attempt!');
}
?>
<p>
<table width="95%">
<tr>
<td colspan="4" class="text">
<?php
echo "<p><strong>Latest ".$filesphp['type']." release: PvPGN ".$filesphp['version']."</strong></p>";
?>
</td>
</tr>
<tr>
<td colspan="4">
</td>
</tr>
<?php
$query = mysql_query("SELECT filename,description,size FROM downloads_".$filesphp['type']." ORDER BY `order` ASC;",$dbh);
if ($row = mysql_fetch_row($query)) {
	do {
		echo "<tr>\n";
		echo "<td width=\"4%\">\n";
		echo "&nbsp;\n";
		echo "</td><td width=\"40%\">\n";
		echo "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\">\n";
		echo "<tr><td>\n";
		echo "<a href=\"http://download.berlios.de/pvpgn/".$row[0]."\" class=\"downloadlink\">".$row[0]."</a>\n";
		echo "</td></tr>\n";
		echo "</table>\n";
		echo "</td><td class=\"text3\" width=\"40%\">\n";
		echo $row[1]."\n";
		echo "</td><td class=\"text3\" width=\"16%\">\n";
		echo formatsize($row[2])."\n";
		echo "</td>\n";
		echo "</tr>\n";
	} while ($row = mysql_fetch_row($query));
} else {
	echo "<tr><td colspan=\"4\">No files found!</td></tr>\n";
}
?>
</table>
</p>
