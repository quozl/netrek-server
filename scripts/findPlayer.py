#William U. Clark, Jr.
#netrek@wuclark.com
#save player one player's stats from statdump to file
#usage findPlayer.py PNUM input output
#!/usr/bin/env python
import sys

inData = open(sys.argv[2],'r').readlines()
outFile = open(sys.argv[3],'w')
for line in inData:
  if line.startswith('STATS_SP_PLAYER:\t%d\t'%int(sys.argv[1])):
    outFile.write(line.strip()+"\n")
outFile.close()
