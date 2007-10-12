#!/usr/bin/env python

import os, time

TIMEFORMAT = "%a, %b %d %Y" # Fri, Oct 12 2007
TIMEOFFSET = 4800 # 80 minutes (60 regulation + 20 overtime)

def dirtimecomp(a, b):
    return b[1][8] - a[1][8]

dirs = []
for f in os.listdir('.'):
    if f.find("-vs-") == -1:
        continue
    dirs += [[f, os.stat(f)]]
dirs.sort(dirtimecomp)

print "<html><head><title>Clue Game Statistics</title></head>"
print "<body><center><h2>Clue Game Statistics</h2></center>"
print "<hr width=25%><br>"
print "<center><table cellpadding=5 border=1><tr><th><font size=+1>Game Date</font></th><th width=500><font size=+1>Home Team -vs- Away Team</font></th><th><font size=+1>Score</font></th></tr>"

for d in dirs:
    url = d[0].replace("?", "%3f")
    name, score = d[0].rsplit(" (", 1)
    score = score.replace(")", "")
    date = time.strftime(TIMEFORMAT, time.localtime(d[1][8] - TIMEOFFSET))
    print "<tr><td>%s</td><td width=500><a href=\"%s/\">%s</a></td><td>%s</td></tr>" % (date, url, name, score)

print "</table></center></body></html>"